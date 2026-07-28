#include <iostream>
#include <fstream>
#include <set>
#include <map>
#include <vector>
#include <queue>
#include <sstream>
#include <random>
#include <algorithm>

#include <stdio.h>

#include <omp.h>

#include "visitorder.h"
#include "scoreplacement.h"
#include "parameters.h"
#include "PatchSpringSimulation.hpp"

void LoadAnnealState(std::set<int> &state)
{
	{
		int i;
			
		std::ifstream is(OUTPUT_DIR "/annealState.csv");
		while(is>>i)
		{
			state.insert(i);
		}
	}
}

// Tunable parameters
const double HEAT_DECAY = 0.85;          // exponential decay applied each iteration
const double HEAT_INCREMENT = 1.0;       // added per appearance in patchesInvolved
const double EXPLORATION_EPSILON = 0.50; // floor weight so cold/unseen patches still get proposed

// Decays existing heat first (so old evidence fades)
void DecayPatchHeat(std::map<int,double> &patchHeat,
                      double decay = HEAT_DECAY, double increment = HEAT_INCREMENT)
{
	for (auto &kv : patchHeat)
		kv.second *= decay;
}

// Call once per outer iteration, right after ScorePlacement returns patchesInvolved.
// Patches only accumulate heat while they are *included* (excluded patches can't
// appear in patchesInvolved, since ScorePlacement never sees them) — so heat is a
// direct signal for "this included patch is currently implicated in a mismatch."
void UpdatePatchHeat(std::map<int,double> &patchHeat, std::set<int> &patchesInvolved,
                      double decay = HEAT_DECAY, double increment = HEAT_INCREMENT)
{
	for (auto &kv : patchHeat)
		kv.second *= decay;

	for (int p : patchesInvolved)
		patchHeat[p] += increment;
	
	/*
	std::cout << "Heats" << std::endl;
	for(auto &kv : patchHeat)
	{
		std::cout << kv.first << ":" << kv.second << " ";
	}
	std::cout << std::endl;
	*/
}

// Builds a sampling distribution over patchNums, weighted by heat (normalized against
// the current max so epsilon stays meaningful regardless of absolute scale).
std::discrete_distribution<int> BuildHeatDistribution(std::vector<int> &patchNums,
                                                        std::map<int,double> &patchHeat,
                                                        double epsilon = EXPLORATION_EPSILON)
{
	std::vector<double> weights;
	weights.reserve(patchNums.size());

	double maxHeat = 0.0;
	for (int p : patchNums)
	{
		auto it = patchHeat.find(p);
		if (it != patchHeat.end() && it->second > maxHeat)
			maxHeat = it->second;
	}

	for (int p : patchNums)
	{
		auto it = patchHeat.find(p);
		double h = (it != patchHeat.end()) ? it->second : 0.0;
		weights.push_back(epsilon + (maxHeat > 0.0 ? h / maxHeat : 0.0));
	}

	return std::discrete_distribution<int>(weights.begin(), weights.end());
}

// size is the maximum number of patches to be added or removed
void MutateStateOld(std::set<int> &newState, std::vector<int> &patchNums, std::set<int> &badPatches, int size)
{
	int addRemove = std::rand()%2;
		
	int numAddRemove = 1+(std::rand()%size);
	
	if (addRemove || newState.size()==0)
	{
		for(int j=0; j<numAddRemove; j++)
		{
			// 10 attempts in case patch is already in badPatches or newState
			// (we expected that most patches won't be)
			for(int i=0; i<10; i++)
			{
				// pick random patch and add it to newState
				int rp = std::rand()%patchNums.size();

				if (badPatches.count(patchNums[rp])==0 && newState.count(patchNums[rp])==0)
				{
					newState.insert(patchNums[rp]);
					printf("Added patch to state\n");
					break;
				}
			}
		}
	}
	
	else if (newState.size()>0)
	{
		for(int j=0; j<numAddRemove && newState.size()>0; j++)
		{
			auto it = std::next(newState.begin(), rand()%newState.size()); // walk to a random position, O(n)
			newState.erase(it); 
			printf("Removed patch from state\n");
		}
	}
}

// size is the maximum number of patches to be added or removed.
// Additions are biased toward high-heat patches (likely causing mismatches).
// Removals are biased toward low-heat patches already in newState (exclusions
// that aren't currently "earning their keep" get tested for removal first).
void MutateState(std::set<int> &newState, std::vector<int> &patchNums, std::set<int> &badPatches,
                  std::map<int,double> &patchHeat, int size, std::mt19937 &rng)
{
	std::uniform_int_distribution<int> coin(0,1);
	int addRemove = coin(rng);

	std::uniform_int_distribution<int> howMany(1,size);
	int numAddRemove = howMany(rng);

	ostringstream message;
	
	if (addRemove || newState.size()==0)
	{
		message << "Added: ";
		
		std::discrete_distribution<int> dist = BuildHeatDistribution(patchNums, patchHeat);

		for(int j=0; j<numAddRemove; j++)
		{
			for(int i=0; i<10; i++)
			{
				int rp = dist(rng);

				if (badPatches.count(patchNums[rp])==0 && newState.count(patchNums[rp])==0)
				{
					newState.insert(patchNums[rp]);
					double h = patchHeat.count(patchNums[rp]) ? patchHeat[patchNums[rp]] : 0.0;
					message << patchNums[rp] << "(" << h << ") ";
					break;
				}
			}
		}
	}
	else if (newState.size()>0)
	{
		message << "Removed: ";
		
		std::vector<int> stateVec(newState.begin(), newState.end());
		std::vector<double> removeWeights;
		removeWeights.reserve(stateVec.size());
		for (int p : stateVec)
		{
			double h = patchHeat.count(p) ? patchHeat[p] : 0.0;
			removeWeights.push_back(1.0 / (1.0 + h)); // low heat -> more likely removed
		}
		std::discrete_distribution<int> removeDist(removeWeights.begin(), removeWeights.end());

		for(int j=0; j<numAddRemove && newState.size()>0; j++)
		{
			std::vector<int> currentVec(newState.begin(), newState.end());
			if (currentVec.empty()) break;

			// rebuild dist each draw since newState shrinks; cheap relative to full eval
			std::vector<double> w;
			w.reserve(currentVec.size());
			for (int p : currentVec)
			{
				double h = patchHeat.count(p) ? patchHeat[p] : 0.0;
				w.push_back(1.0 / (1.0 + h));
			}
			std::discrete_distribution<int> d(w.begin(), w.end());
			int idx = d(rng);

			int patchToRemove = currentVec[idx];
			newState.erase(patchToRemove);
			message << patchToRemove << " ";
		}
	}
	
	std::cout << message.str() << std::endl;
}

bool AcceptOld(float score, float currentScore, int iters, int maxIters)
{
	float scoreDiff = score - currentScore;
	
	if (scoreDiff>0.0)
		return true;
	
	// Acceptance probability depends on scoreDiff and temperature and chance
	
	// temp between 0 and 1
	float temp = (float)(maxIters - iters) / (float)maxIters;

	// maximum score difference that could be accepted if temp = 1.0
	int maxAccept = 200;
	
	if (temp * (float)(rand()%maxAccept) > -scoreDiff)
	{
		return true;
	}
	
	return false;
}


// T0 supplied externally (see calibration below). Reaches exactly 0 at iters==maxIters.
float TemperatureAt(int iters, int maxIters, float T0)
{
	float frac = (float)(maxIters - iters) / (float)maxIters;
	if (frac < 0.0f) frac = 0.0f;
	return T0 * frac;
}

// scoreDiff = score - currentScore (negative = worse candidate)
// T = current temperature, must be > 0 when called; caller should special-case T<=0
bool Accept(float scoreDiff, float T, std::mt19937 &rng)
{
	if (scoreDiff > 0.0f)
		return true;

	if (T <= 0.0f)
		return false; // frozen: only accept strict improvements

	std::uniform_real_distribution<float> u(0.0f, 1.0f);
	float p = std::exp(scoreDiff / T); // scoreDiff <= 0 here, so p in (0,1]
	return u(rng) < p;
}

// Runs the full pipeline for a candidate state: builds effective bad patches,
// computes visit order, writes alignment order, runs the spring simulation,
// scores the placement, and updates patch heat from the mismatches found.
// Returns the score. This is the single expensive operation in the whole
// algorithm — both calibration sampling and the main loop route through here.
float EvaluateState(AlignmentMap *am, std::map<int,Patch> *patches,
                     std::set<int> &state, std::set<int> &badPatches,
                     std::set<std::pair<int,int>> &manualBadRel,
                     std::set<int> &patchesInvolved)
{
	std::set<int> effectiveBadPatches;

	std::set_union(badPatches.begin(), badPatches.end(),
	               state.begin(), state.end(),
	               std::inserter(effectiveBadPatches, effectiveBadPatches.begin()));

	std::vector<int> patchOrder;
	std::vector<std::pair<int,alignment>> alignmentOrder;
	std::vector<std::vector<std::string>> alignmentOrderDash;
	std::map<int,affineTx> patchPositions;
	std::map<int,std::set<int> > neighbourList;

	MakeVisitOrder(am, patches, effectiveBadPatches, manualBadRel,
	               patchOrder, alignmentOrder, patchPositions, neighbourList);

				   
	{
		for(auto &a : alignmentOrder)
		{
			std::vector<std::string> row;
			auto toStr = [](auto val)
			{
				std::ostringstream oss;
				oss << val;
				return oss.str();
			};

			row.push_back(toStr(a.first));
			row.push_back(toStr((*patches)[a.first].radius));
			row.push_back(toStr(std::get<0>(a.second)));
			row.push_back(toStr(std::get<7>(a.second)));
			row.push_back(toStr(std::get<8>(a.second)));
			row.push_back(toStr(std::get<9>(a.second)));
			row.push_back(toStr(std::get<10>(a.second)));
			row.push_back(toStr(std::get<11>(a.second)));
			row.push_back(toStr(std::get<12>(a.second)));

			alignmentOrderDash.push_back(std::move(row));
		}
	}

	//printf("Running patchsprings...\n");
	{
		PatchSpringSimulation pss(QUADMESH_SIZE,OUTPUT_DIR,false);

		pss.loadPatchVolCoords(OUTPUT_DIR "/patchVolCoords.csv");

		//printf("Loading patches for patchsprings...\n");
		pss.loadPatches(alignmentOrderDash, patches->size());

		//printf("Running patchsprings...\n");
		pss.run(50);
		//printf("Finished patchsprings...\n");
	
	    //printf("Finished running patchsprings\n");


	    // Get patch positions from pss and set them in *patches
		for (auto i : patchOrder)
		{
			(*patches)[i].UnsetPosition();
			float x,y,angle;
            pss.GetPatchPosition(i,x,y,angle);
			(*patches)[i].SetPosition(x,y,angle);
		}
	}
	
    std::set<int> patchesToColour;
    std::set<std::pair<int,int>> manualGoodRel;
	
	float score = ScorePlacement(am, patches, patchOrder, patchesToColour,
	                              manualGoodRel, patchesInvolved, 30, 10, false);

	return score;
}

// Returns T0 such that the mean acceptance probability over the sampled
// negative deltas is approximately targetAcceptance (e.g. 0.5).
float CalibrateT0(const std::vector<float> &negativeDeltas, float targetAcceptance = 0.5f)
{
	if (negativeDeltas.empty())
		return 1.0f; // fallback; shouldn't normally happen

	auto meanAcceptance = [&](float T) -> float
	{
		double sum = 0.0;
		for (float d : negativeDeltas)
			sum += std::exp(d / T); // d <= 0
		return (float)(sum / negativeDeltas.size());
	};

	// Bisection on T in a wide bracket. meanAcceptance(T) is monotonically
	// increasing in T (higher T -> higher acceptance), so bisection is safe.
	float lo = 1e-4f, hi = 1e6f;

	// Expand hi if even a huge T can't reach target (deltas all ~0)
	for (int i = 0; i < 100 && meanAcceptance(hi) < targetAcceptance; i++)
		hi *= 10.0f;

	for (int iter = 0; iter < 60; iter++) // plenty for float precision
	{
		float mid = 0.5f * (lo + hi);
		if (meanAcceptance(mid) < targetAcceptance)
			lo = mid;
		else
			hi = mid;
	}

	return 0.5f * (lo + hi);
}

struct CandidateResult
{
	std::set<int> state;
	float score = -std::numeric_limits<float>::infinity();
	std::set<int> patchesInvolved;
};

void Anneal(AlignmentMap *am, std::map<int,Patch> *patches, std::vector<int> patchNums,
            std::set<int> badPatches, std::set<std::pair<int,int>> manualBadRel,
            std::set<int> badBridges, int iterations, int N)
{
	std::set<int> currentState;
	std::set<int> newState;
	LoadAnnealState(currentState);
	std::map<int,double> patchHeat;
	std::mt19937 rng(std::random_device{}());
	for(auto i : badBridges)
	{
		patchHeat[i]=10.0;
	}

	int maxIters = iterations;
	
	
	// ---- Calibration pass (serial) ----
	std::set<int> patchesInvolved;
	
	printf("Calibrating T0...\n");
	float baselineScore = EvaluateState(am, patches, currentState, badPatches,
										 manualBadRel, patchesInvolved);
	std::vector<float> negativeDeltas;
	int numCalibrationSamples = 8;
	for (int i = 0; i < numCalibrationSamples; i++)
	{
		patchesInvolved.clear();
		
		std::set<int> trialState = currentState;
		MutateState(trialState, patchNums, badPatches, patchHeat, 5, rng);
		float trialScore = EvaluateState(am, patches, trialState, badPatches,
										  manualBadRel, patchesInvolved);
		float delta = trialScore - baselineScore;
		printf("Calibration sample %d: delta=%f\n", i, delta);
		if (delta < 0.0f)
			negativeDeltas.push_back(delta);
	}
	float T0 = CalibrateT0(negativeDeltas, 0.5f);
	printf("Calibrated T0 = %f (from %zu negative samples)\n", T0, negativeDeltas.size());
	
	
	// ---- Main loop ----
	float currentScore = baselineScore, score = 0;
	std::set<int> bestState = currentState;
	float bestScore = currentScore;
	std::vector<float> scoreLog, currentScoreLog, stateLength;

	std::vector<CandidateResult> candidates(N);

	for(int iters = 0; iters<maxIters; iters++)
	{		
		float T = TemperatureAt(iters, maxIters, T0);

		std::cout << "-------- Annealing iteration " << iters << ", T=" << T << " : Best score=" << bestScore
		           << ", state size=" << (int)bestState.size() << " --------" << std::endl;

		// Draw N seeds sequentially from the master RNG so each thread gets its
		// own independent, reproducible generator (std::mt19937 is not thread-safe
		// to share across threads).
		std::vector<unsigned int> seeds(N);
		for(int n=0; n<N; n++)
			seeds[n] = rng();

		#pragma omp parallel for schedule(dynamic)
		for(int n=0; n<N; n++)
		{
			std::mt19937 localRng(seeds[n]);
			std::set<int> localState = currentState;
			if (iters != 0)
			{
				#pragma omp critical(mutate_state)
				{
					MutateState(localState, patchNums, badPatches, patchHeat, 5, localRng);
				}
			}

			std::set<int> localPatchesInvolved;
			// NOTE: EvaluateState needs a variant that fills patchesInvolved
			// instead of mutating patchHeat internally, so it's safe to call
			// concurrently (concurrent reads of patchHeat are fine, concurrent
			// writes are not).
			float localScore = EvaluateState(am, patches, localState, badPatches,
			                                  manualBadRel, localPatchesInvolved);

			candidates[n].state = std::move(localState);
			candidates[n].score = localScore;
			candidates[n].patchesInvolved = std::move(localPatchesInvolved);
		}

		// Pick the best of the N candidates
		int bestIdx = 0;
		for(int n=1; n<N; n++)
		{
			if (candidates[n].score > candidates[bestIdx].score)
				bestIdx = n;
		}
		newState = candidates[bestIdx].state;
		score = candidates[bestIdx].score;

		printf("State size = %d, score = %f (best of %d candidates)\n",
		       (int)newState.size(), score, N);

		DecayPatchHeat(patchHeat);
		// Now that all N threads have finished, merge every run's
		// patchesInvolved into patchHeat (serial, no race).
		for(int n=0; n<N; n++)
		{
			UpdatePatchHeat(patchHeat, candidates[n].patchesInvolved);
		}

		if (iters==0 || Accept(score - currentScore, T, rng))
		{
			printf("Accepted (T=%f)\n", T);
			currentScore = score;
			currentState = newState;
		}
		if (score > bestScore)
		{
			bestScore = score;
			bestState = newState;
		}
		scoreLog.push_back(score);
		currentScoreLog.push_back(currentScore);
		stateLength.push_back(newState.size());
	}
	{
		std::ofstream os(OUTPUT_DIR "/annealStats.csv");
		for(int i = 0; i<(int)scoreLog.size(); i++)
		{
			os << scoreLog[i] << "," << currentScoreLog[i] << "," << stateLength[i] << std::endl;
		}
	}

	printf("Best score = %f\n", bestScore);
	{
		std::ofstream os(OUTPUT_DIR "/annealState_out.csv");
		for(auto i : bestState)
		{
			os << i << std::endl;
		}
	}
}
/*
void Anneal(AlignmentMap *am, std::map<int,Patch> *patches, std::vector<int> patchNums, std::set<int> badPatches, std::set<std::pair<int,int>> manualBadRel, std::set<int> badBridges, int iterations)
{
	std::set<int> currentState;
	std::set<int> newState;

	LoadAnnealState(currentState);

	std::map<int,double> patchHeat;
	std::mt19937 rng(std::random_device{}());

	for(auto i : badBridges)
	{
		patchHeat[i]=10.0;
	}
	
	int maxIters = iterations;

	// ---- Calibration pass ----
	printf("Calibrating T0...\n");
	float baselineScore = EvaluateState(am, patches, currentState, badPatches,
	                                     manualBadRel, patchHeat);

	std::vector<float> negativeDeltas;
	int numCalibrationSamples = 8;
	for (int i = 0; i < numCalibrationSamples; i++)
	{
		std::set<int> trialState = currentState;
		MutateState(trialState, patchNums, badPatches, patchHeat, 5, rng);

		float trialScore = EvaluateState(am, patches, trialState, badPatches,
		                                  manualBadRel, patchHeat);

		float delta = trialScore - baselineScore;
		printf("Calibration sample %d: delta=%f\n", i, delta);
		if (delta < 0.0f)
			negativeDeltas.push_back(delta);
	}

	float T0 = CalibrateT0(negativeDeltas, 0.5f);
	printf("Calibrated T0 = %f (from %zu negative samples)\n", T0, negativeDeltas.size());

	// ---- Main loop ----
	float currentScore = baselineScore, score = 0;
	std::set<int> bestState = currentState;
	float bestScore = currentScore;

	std::vector<float> scoreLog, currentScoreLog, stateLength;

	for(int iters = 0; iters<maxIters; iters++)
	{
		std::cout << "-------- Annealing iteration " << iters << " : Best score=" << bestScore << ", state size=" << (int)bestState.size() << " --------" << std::endl;
		newState = currentState;

		if (iters != 0)
		{
			MutateState(newState, patchNums, badPatches, patchHeat, 5, rng);
		}

		score = EvaluateState(am, patches, newState, badPatches, manualBadRel, patchHeat);

		printf("State size = %d, score = %f\n",(int)newState.size(),score);

		float T = TemperatureAt(iters, maxIters, T0);

		if (iters==0 || Accept(score - currentScore, T, rng))
		{
			printf("Accepted (T=%f)\n", T);
			currentScore = score;
			currentState = newState;
		}

		if (score > bestScore)
		{
			bestScore = score;
			bestState = newState;
		}

		scoreLog.push_back(score);
		currentScoreLog.push_back(currentScore);
		stateLength.push_back(newState.size());
	}

	{
		std::ofstream os(OUTPUT_DIR "/annealStats.csv");
		for(int i = 0; i<(int)scoreLog.size(); i++)
		{
			os << scoreLog[i] << "," << currentScoreLog[i] << "," << stateLength[i] << std::endl;
		}
	}
	
	printf("Best score = %f\n", bestScore);
	{
		std::ofstream os(OUTPUT_DIR "/annealState_out.csv");
		
		for(auto i : bestState)
		{
			os << i << std::endl;
		}
	}

}
*/
/*
void Anneal(void)
{
	AlignmentMap *am = new AlignmentMap;
	std::map<int,Patch> *patches = new std::map<int,Patch>;
	
	printf("Loading patches and relationships...\n");
	LoadPatchesAndRelationships(patches,am,PATCH_LIMIT);

	AugmentAlignmentMap(*am);

	std::vector<int> patchNums;
	for(auto &i : *patches)
		patchNums.push_back(i.first);
	
	std::set<int> badPatches;
	std::set<std::pair<int,int>> manualBadRel;

	LoadBadPatches(badPatches,manualBadRel);

	// a state vector is a set of additional bad patches
	std::set<int> currentState,newState,bestState;

	LoadAnnealState(currentState);
	
	std::map<int,double> patchHeat;      // NEW: persists across iterations
	std::mt19937 rng(std::random_device{}());  // NEW: proper RNG for discrete_distribution
	
	float currentScore=0,score=0,bestScore=0;
	
	std::vector<float> scoreLog,currentScoreLog;
	
	int maxIters = 2000;
	for(int iters = 0; iters<maxIters; iters++)
	{
		newState = currentState;
		
		if (iters != 0)
		{			
			MutateState(newState,patchNums,badPatches,patchHeat,1+(10*(maxIters-iters))/maxIters,rng);  // CHANGED
		}
		
		std::set<int> effectiveBadPatches;
		
		std::set_union(badPatches.begin(), badPatches.end(),
					   newState.begin(), newState.end(),
                       std::inserter(effectiveBadPatches, effectiveBadPatches.begin()));
					   
		std::vector<int> patchOrder;
		std::vector<std::pair<int,alignment>> alignmentOrder;
		std::map<int,affineTx> patchPositions;
		std::map<int,std::set<int> > neighbourList;
		
		MakeVisitOrder(am,patches,effectiveBadPatches,manualBadRel,patchOrder,alignmentOrder,patchPositions,neighbourList);

		{
			ofstream os(OUTPUT_DIR "/alignmentorder.txt");
			
			for(auto &a : alignmentOrder)
			{
				os << a.first << " "
				   << (*patches)[a.first].radius << " "			
				   << std::get<0>(a.second) << " "
				   << std::get<7>(a.second) << " "
				   << std::get<8>(a.second) << " "
				   << std::get<9>(a.second) << " "
				   << std::get<10>(a.second) << " "
				   << std::get<11>(a.second) << " "
				   << std::get<12>(a.second) << " "
				   << endl;
			}
		}
	
	    printf("Running patchsprings...\n");
		{
			PatchSpringSimulation pss(QUADMESH_SIZE,OUTPUT_DIR);
			
			pss.loadPatchVolCoords(OUTPUT_DIR "/patchVolCoords.csv");
			
			std::vector<std::vector<std::string>> alignmentOrderDash;
			{
				std::ifstream f(OUTPUT_DIR "/alignmentorder.txt");
				if (!f) {
					std::cerr << "Could not open alignmentorder.txt\n";
				}
				std::string line;
				while (std::getline(f, line)) {
					alignmentOrderDash.push_back(splitOnSpaceDropLast(line));
				}
			}

			printf("Loading patches for patchsprings...\n");
			pss.loadPatches(alignmentOrderDash, patches->size());
			
			printf("Running patchsprings...\n");
		    pss.run(50);
			printf("Finished patchsprings...\n");

		}
	    printf("Finished running patchsprings\n");
		
		std::set<int> patchesToColour;
		std::set<std::pair<int,int>> manualGoodRel;
		std::set<int> patchesInvolved;
		
		score = ScorePlacement(am, patches, patchOrder, patchesToColour, manualGoodRel, patchesInvolved, 30,10,false);

		UpdatePatchHeat(patchHeat, patchesInvolved);  // NEW
		
		printf("Score = %f\n",score);
	
		if (iters==0 || Accept(score,currentScore,iters,maxIters,T0,rng))
		{
			printf("New best score\n");
			currentScore = score;
			currentState = newState;
		}
		
		if (iters==0 || score<bestScore)
		{
			bestScore = score;
			bestState = newState;
		}
		
        scoreLog.push_back(score);	
        currentScoreLog.push_back(currentScore);	
	}
	
	printf("Scores:\n");
	for(int i = 0; i<scoreLog.size(); i++)
	{
		printf("%f,%f\n",scoreLog[i],currentScoreLog[i]);
	}
	
	printf("State with best score (%f):\n",bestScore);
	for(auto i : bestState)
	{
		printf("%d\n",i);
	}
}
*/
