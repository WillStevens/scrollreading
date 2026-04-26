#!/usr/bin/env python3
"""
visualise_graph.py  –  Render a patch-graph CSV as a graphical image.

Usage:
    python visualise_graph.py <csv_file> [output_image]

Arguments:
    csv_file        Path to the CSV file (patch1,patch2,alignment per row).
    output_image    Optional output path (e.g. graph.png, graph.svg).
                    Defaults to <csv_file_stem>_graph.png.
                    If omitted AND a display is available the graph is also
                    shown interactively.

Dependencies (install once):
    pip install networkx matplotlib
"""

import sys
import os
import csv
import math
import argparse

import networkx as nx
import matplotlib
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import matplotlib.colors as mcolors


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def load_graph(path: str) -> nx.Graph:
    """Parse the CSV and return an undirected NetworkX graph."""
    G = nx.Graph()
    with open(path, newline="") as f:
        for row in csv.reader(f):
            if len(row) < 2:
                continue
            try:
                p1 = int(row[0].strip())
                p2 = int(row[1].strip())
            except ValueError:
                continue          # skip header / non-numeric rows
            alignment = row[2].strip() if len(row) >= 3 else ""
            G.add_edge(p1, p2, alignment=alignment)
    return G


def choose_layout(G: nx.Graph):
    """Pick a layout algorithm appropriate to the graph's size."""
    n = G.number_of_nodes()
    if n <= 50:
        # Spring layout looks great for small graphs
        return nx.spring_layout(G, seed=42, k=2.5 / math.sqrt(max(n, 1)))
    elif n <= 300:
        # Kamada-Kawai preserves distances well for medium graphs
        return nx.kamada_kawai_layout(G)
    else:
        # For large graphs fall back to the fast spectral / spring hybrid
        pos = nx.spectral_layout(G)
        return nx.spring_layout(G, pos=pos, seed=42, iterations=20,
                                k=1.5 / math.sqrt(max(n, 1)))


def degree_colours(G: nx.Graph, node_order):
    """Return an RGBA array coloured by node degree (viridis palette)."""
    degrees = [G.degree(v) for v in node_order]
    max_deg = max(degrees) if degrees else 1
    norm = mcolors.Normalize(vmin=0, vmax=max_deg)
    cmap = cm.viridis
    return [cmap(norm(d)) for d in degrees], degrees


# ---------------------------------------------------------------------------
# Main rendering
# ---------------------------------------------------------------------------

def render(G: nx.Graph, output_path: str | None, show: bool) -> None:
    n_nodes = G.number_of_nodes()
    n_edges = G.number_of_edges()

    # --- Figure size scales with graph ---
    fig_side = max(8, min(32, 6 + math.sqrt(n_nodes) * 0.9))
    fig, ax = plt.subplots(figsize=(fig_side, fig_side),
                           facecolor="#0d1117")
    ax.set_facecolor("#0d1117")
    ax.axis("off")

    pos = choose_layout(G)
    node_order = list(G.nodes())

    colours, degrees = degree_colours(G, node_order)

    # Node size: shrink for large graphs
    node_size = max(20, min(600, 1200 / math.sqrt(max(n_nodes, 1))))
    # Edge width
    edge_width = max(0.3, min(1.5, 60 / max(n_edges, 1)))
    # Font size
    label_size = max(4, min(11, 180 / math.sqrt(max(n_nodes, 1))))
    draw_labels = n_nodes <= 200

    # --- Draw edges first (behind nodes) ---
    nx.draw_networkx_edges(
        G, pos,
        ax=ax,
        edge_color="#4a6fa5",
        alpha=0.45,
        width=edge_width,
    )

    # --- Draw nodes ---
    nx.draw_networkx_nodes(
        G, pos,
        nodelist=node_order,
        ax=ax,
        node_color=colours,
        node_size=node_size,
        linewidths=0.6,
        edgecolors="#ffffff",
    )

    # --- Labels (only when not too crowded) ---
    if draw_labels:
        nx.draw_networkx_labels(
            G, pos,
            ax=ax,
            font_size=label_size,
            font_color="#e6edf3",
            font_family="monospace",
        )

    # --- Colour-bar legend for degree ---
    max_deg = max(degrees) if degrees else 1
    sm = cm.ScalarMappable(
        cmap=cm.viridis,
        norm=mcolors.Normalize(vmin=0, vmax=max_deg)
    )
    sm.set_array([])
    cbar = fig.colorbar(sm, ax=ax, fraction=0.025, pad=0.01, aspect=30)
    cbar.set_label("Node degree", color="#8b949e", fontsize=9,
                   fontfamily="monospace")
    cbar.ax.yaxis.set_tick_params(color="#8b949e")
    plt.setp(cbar.ax.yaxis.get_ticklabels(), color="#8b949e",
             fontfamily="monospace", fontsize=8)
    cbar.outline.set_edgecolor("#30363d")

    # --- Title / stats ---
    components = nx.number_connected_components(G)
    title = (f"{n_nodes} vertices · {n_edges} edges · "
             f"{components} component{'s' if components != 1 else ''}")
    ax.set_title(title, color="#8b949e", fontsize=10,
                 fontfamily="monospace", pad=10)

    plt.tight_layout(pad=0.5)

    if output_path:
        dpi = 150 if n_nodes <= 500 else 100
        plt.savefig(output_path, dpi=dpi, bbox_inches="tight",
                    facecolor=fig.get_facecolor())
        print(f"Graph saved to: {output_path}")

    if show:
        plt.show()

    plt.close(fig)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Visualise a patch-graph CSV file.")
    parser.add_argument("csv_file",
                        help="Input CSV file (patch1,patch2,alignment)")
    parser.add_argument("output_image", nargs="?", default=None,
                        help="Output image path (png/svg/pdf). "
                             "Defaults to <csv_stem>_graph.png")
    args = parser.parse_args()

    if not os.path.isfile(args.csv_file):
        print(f"Error: file not found: {args.csv_file}", file=sys.stderr)
        sys.exit(1)

    # Determine output path
    if args.output_image:
        out_path = args.output_image
    else:
        stem = os.path.splitext(os.path.basename(args.csv_file))[0]
        out_path = f"{stem}_graph.png"

    # Decide whether to attempt interactive display
    show_interactive = args.output_image is None
    if show_interactive:
        try:
            import matplotlib
            matplotlib.use("TkAgg")      # try a GUI backend
        except Exception:
            show_interactive = False     # headless – just save

    print(f"Loading graph from: {args.csv_file}")
    G = load_graph(args.csv_file)
    print(f"  {G.number_of_nodes()} vertices, {G.number_of_edges()} edges, "
          f"{nx.number_connected_components(G)} component(s)")

    render(G, out_path, show=show_interactive)


if __name__ == "__main__":
    main()