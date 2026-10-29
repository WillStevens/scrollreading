#include <iostream>
#include <fstream>
#include <set>
#include <unordered_map>
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

// WS 2026-10-14 : addition and removal are now asymmetric, addition adds patch and all of its neighbours, removal only removes single patches. It is hoped that this will increase the rate of state space exploration
void MutateState(std::set<int> &newState, std::vector<int> &patchNums, std::set<int> &badPatches,
                  std::map<int,double> &patchHeat, std::map<int,std::set<int> > &neighbourList, int size, std::mt19937 &rng)
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
			bool addedAny = false;
			for(int i=0; i<10; i++)
			{
				// Pick a patch
				int ch = dist(rng);

				if (badPatches.count(patchNums[ch])==0 && newState.count(patchNums[ch])==0)
				{
					newState.insert(patchNums[ch]);
					double h = patchHeat.count(patchNums[ch]) ? patchHeat[patchNums[ch]] : 0.0;
					message << patchNums[ch] << "(" << h << ") ";
					addedAny = true;
				}

				// Add all of its neighbours to the state
				if (0)
				for(auto rp : neighbourList[ch])
				{
					if (badPatches.count(patchNums[rp])==0 && newState.count(patchNums[rp])==0)
					{
						newState.insert(patchNums[rp]);
						double h = patchHeat.count(patchNums[rp]) ? patchHeat[patchNums[rp]] : 0.0;
						message << patchNums[rp] << "(" << h << ") ";
						addedAny = true;
					}
				}
				
				if (addedAny) break;
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
					 AlignmentScoreMap &patchAlignmentScores,
                     std::set<int> &patchesInvolved,
					 std::map<int,std::set<int> > &neighbourList)
{
	std::set<int> effectiveBadPatches;

	std::set_union(badPatches.begin(), badPatches.end(),
	               state.begin(), state.end(),
	               std::inserter(effectiveBadPatches, effectiveBadPatches.begin()));

	std::vector<int> patchOrder;
	std::vector<std::pair<int,alignment>> alignmentOrder;
	std::map<int,affineTx> patchPositions;
	
	neighbourList.clear();
	MakeVisitOrder(am, patches, effectiveBadPatches, manualBadRel,
	               patchOrder, alignmentOrder, patchPositions, neighbourList,patchAlignmentScores,false,false);

				   
	std::unordered_map<int,std::tuple<float,float,float,float,float>> patchPositionsXYA;
	
	//printf("Running patchsprings...\n");
	{
		PatchSpringSimulation pss(QUADMESH_SIZE,OUTPUT_DIR,false);

		pss.loadPatchVolCoords(OUTPUT_DIR "/patchVolCoords.csv");

		//printf("Loading patches for patchsprings...\n");
		auto radiusOf = [patches](int id) -> double {
			return patches->find(id)->second.radius;   // find, not operator[]: safe for concurrent reads
		};
		pss.loadPatches(alignmentOrder, patches->size(), radiusOf);
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
			
			patchPositionsXYA[i]=std::tuple<float,float,float,float,float>(x,y,angle,sin(angle),cos(angle));
			
			//printf("%d %f %f %f\n",i,x,y,angle);
		}
	}
	
    std::set<int> patchesToColour;
    std::set<std::pair<int,int>> manualGoodRel;
	
	float score = ScorePlacementAreaAndIncon(am, patches, patchPositionsXYA,patchOrder, patchesToColour,
	                              manualGoodRel, patchesInvolved, 30, 10, false, false);

	return score;
}

// Runs the full pipeline for a candidate state: builds effective bad patches,
// computes visit order, writes alignment order, runs the spring simulation,
// scores the placement, and updates patch heat from the mismatches found.
// Returns the score. This is the single expensive operation in the whole
// algorithm — both calibration sampling and the main loop route through here.
float EvaluateStateAll(int numComponents, AlignmentMap *am, std::map<int,Patch> *patches,
                     std::set<int> &state, std::set<int> &badPatches,
                     std::set<std::pair<int,int>> &manualBadRel,
					 AlignmentScoreMap &patchAlignmentScores,
                     std::set<int> &patchesInvolved,
					 std::map<int,std::set<int> > &neighbourList)
{
	std::set<int> effectiveBadPatches;

	std::set_union(badPatches.begin(), badPatches.end(),
	               state.begin(), state.end(),
	               std::inserter(effectiveBadPatches, effectiveBadPatches.begin()));

	std::vector<std::vector<int>> patchOrders;
	std::vector<std::vector<std::pair<int,alignment>>> alignmentOrders;
	std::vector<std::map<int,affineTx>> patchPositionss;
	neighbourList.clear();

	MakeVisitOrders(numComponents,am, patches, effectiveBadPatches, manualBadRel,
	               patchOrders, alignmentOrders, patchPositionss, neighbourList, patchAlignmentScores, false,false);

	float score = 0.0;
	int componentIndex = -1;
	for(auto &alignmentOrder : alignmentOrders)			   
	{
		componentIndex++;
	
		std::unordered_map<int,std::tuple<float,float,float,float,float>> patchPositionsXYA;
	
		//printf("Running patchsprings...\n");
		{
			PatchSpringSimulation pss(QUADMESH_SIZE,OUTPUT_DIR,componentIndex,false);

			//pss.loadPatchVolCoords(OUTPUT_DIR "/patchVolCoords.csv");

			//printf("Loading patches for patchsprings...\n");
			auto radiusOf = [patches](int id) -> double {
				return patches->find(id)->second.radius;   // find, not operator[]: safe for concurrent reads
			};
			pss.loadPatches(alignmentOrder, patches->size(), radiusOf);
			
			//printf("Running patchsprings...\n");
			float pssScore = pss.run(50);
			printf("Patchsprings score:%f\n",pssScore);
			score -= pssScore*STRETCH_SCORE_FACTOR;
			
			//printf("Finished patchsprings...\n");
	
	        //printf("Finished running patchsprings\n");


	        // Get patch positions from pss and set them in *patches
		    for (auto i : patchOrders[componentIndex])
		    {
				(*patches)[i].UnsetPosition();
				float x,y,angle;
				pss.GetPatchPosition(i,x,y,angle);
			
				patchPositionsXYA[i]=std::tuple<float,float,float,float,float>(x,y,angle,sin(angle),cos(angle));
			
			//printf("%d %f %f %f\n",i,x,y,angle);
			}
	
	
			std::set<int> patchesToColour;
			std::set<std::pair<int,int>> manualGoodRel;
	
			score += ScorePlacementAreaAndIncon(am, patches, patchPositionsXYA,patchOrders[componentIndex], patchesToColour,
	                              manualGoodRel, patchesInvolved, 30, 10, false, false);
		}
	}
	
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
	std::map<int,std::set<int> > neighbourList;
};

void Anneal(AlignmentMap *am, std::map<int,Patch> *patches, std::vector<int> patchNums,
            std::set<int> badPatches, std::set<std::pair<int,int>> manualBadRel,
            std::set<int> badBridges, AlignmentScoreMap &patchAlignmentScores, int iterations, int N, float initT0=-1.0)
{
	std::set<int> currentState;
	std::set<int> newState;
	LoadAnnealState(currentState);
	std::map<int,double> patchHeat;
	std::mt19937 rng(std::random_device{}());
	std::map<int,std::set<int> > neighbourList;
	
	for(auto i : badBridges)
	{
		patchHeat[i]=10.0;
	}

	int maxIters = iterations;
	
	
	// ---- Calibration pass (serial) ----
	std::set<int> patchesInvolved;

    float T0;

	float baselineScore = EvaluateState(am, patches, currentState, badPatches,
											 manualBadRel, patchAlignmentScores, patchesInvolved, neighbourList);
	
	if (initT0==-1)
	{
		printf("Calibrating T0...\n");
		std::vector<float> negativeDeltas;
		int numCalibrationSamples = 8;
		for (int i = 0; i < numCalibrationSamples; i++)
		{
			patchesInvolved.clear();
			
			std::set<int> trialState = currentState;
			std::map<int,std::set<int> > trialNeighbourList = std::move(neighbourList);
			
			MutateState(trialState, patchNums, badPatches, patchHeat, trialNeighbourList, 20, rng);
			float trialScore = EvaluateState(am, patches, trialState, badPatches,
											  manualBadRel, patchAlignmentScores, patchesInvolved, trialNeighbourList);
			float delta = trialScore - baselineScore;
			printf("Calibration sample %d: delta=%f\n", i, delta);
			if (delta < 0.0f)
				negativeDeltas.push_back(delta);
		}
		T0 = CalibrateT0(negativeDeltas, 0.5f);
	    printf("Calibrated T0 = %f (from %zu negative samples)\n", T0, negativeDeltas.size());
	}
	else
	{
		T0 = initT0;
	}
	
	// ---- Main loop ----
	float currentScore = baselineScore;
	std::set<int> bestState = currentState;
	float bestScore = currentScore;
	std::vector<float> scoreLog, currentScoreLog, stateLength;

	std::vector<CandidateResult> candidates(N);

	int acceptedCount = 0;
	
	for(int iters = 0; iters<maxIters; iters++)
	{		
		if (acceptedCount>=12)
		{
			T0=T0*0.75;
			acceptedCount=0;
		}
		
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
			std::map<int,std::set<int> > localNeighbourList = neighbourList;

			if (iters != 0)
			{
				#pragma omp critical(mutate_state)
				{
					printf("MutateState\n");
					//MutateState(localState, patchNums, badPatches, patchHeat, 15*(T/T0)*0+1, localRng);
					MutateState(localState, patchNums, badPatches, patchHeat, localNeighbourList,15*(T/T0)+1, localRng);
					printf("Done MutateState\n");
				}
			}

			std::set<int> localPatchesInvolved;
			// NOTE: EvaluateState needs a variant that fills patchesInvolved
			// instead of mutating patchHeat internally, so it's safe to call
			// concurrently (concurrent reads of patchHeat are fine, concurrent
			// writes are not).
			float localScore = EvaluateState(am, patches, localState, badPatches,
			                                  manualBadRel, patchAlignmentScores, localPatchesInvolved, localNeighbourList);

			candidates[n].state = std::move(localState);
			candidates[n].score = localScore;
			candidates[n].patchesInvolved = std::move(localPatchesInvolved);
			candidates[n].neighbourList = std::move(localNeighbourList);
		}

		// We don't want to just pick the best of the N candidates or this would reduce
		// the smoothing that simulated annealing depends on - it would be the same as reducing the temperature.
		// Instead we try accepting each one in turn
		
		for(int n=0; n<N; n++)
		{
			DecayPatchHeat(patchHeat);
			UpdatePatchHeat(patchHeat, candidates[n].patchesInvolved);

			if ((iters==0 && n==0) || Accept(candidates[n].score - currentScore, T, rng))
		    {
			    printf("Accepted %d (T=%f)\n",n,T);
			    currentScore = candidates[n].score;
			    currentState = candidates[n].state;
				neighbourList = std::move(candidates[n].neighbourList);
				
				acceptedCount++;
		    }
			else
			{
				acceptedCount = 0;
			}
			
		    if (candidates[n].score > bestScore)
		    {
			    bestScore = candidates[n].score;
			    bestState = candidates[n].state;
		    }
		    scoreLog.push_back(candidates[n].score);
		    currentScoreLog.push_back(currentScore);
		    stateLength.push_back(candidates[n].state.size());

			printf("State size = %d, score = %f\n",
		       (int)candidates[n].state.size(), candidates[n].score);

		}
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

	// Show the output from visit order - useful to see sizes of components.
	{
		std::set<int> effectiveBadPatches;

		std::set_union(badPatches.begin(), badPatches.end(),
					bestState.begin(), bestState.end(),
					std::inserter(effectiveBadPatches, effectiveBadPatches.begin()));

		std::vector<int> patchOrder;
		std::vector<std::pair<int,alignment>> alignmentOrder;
		std::map<int,affineTx> patchPositions;
		std::map<int,std::set<int> > neighbourList;

		MakeVisitOrder(am, patches, effectiveBadPatches, manualBadRel,
	               patchOrder, alignmentOrder, patchPositions, neighbourList, patchAlignmentScores,true);
	}
}

void AnnealAll(int numComponents,AlignmentMap *am, std::map<int,Patch> *patches, std::vector<int> patchNums,
            std::set<int> badPatches, std::set<std::pair<int,int>> manualBadRel,
            std::set<int> badBridges, AlignmentScoreMap &patchAlignmentScores, int iterations, int N, float initT0=-1.0)
{
	std::set<int> currentState;
	std::set<int> newState;
	LoadAnnealState(currentState);
	std::map<int,double> patchHeat;
	std::mt19937 rng(std::random_device{}());
	std::map<int,std::set<int> > neighbourList;

	for(auto i : badBridges)
	{
		patchHeat[i]=10.0;
	}

	int maxIters = iterations;
	
	
	// ---- Calibration pass (serial) ----
	std::set<int> patchesInvolved;

    float T0;

	float baselineScore = EvaluateStateAll(numComponents,am, patches, currentState, badPatches,
											 manualBadRel, patchAlignmentScores, patchesInvolved,neighbourList);
	
	if (initT0==-1)
	{
		printf("Calibrating T0...\n");
		std::vector<float> negativeDeltas;
		int numCalibrationSamples = 8;
		for (int i = 0; i < numCalibrationSamples; i++)
		{
			patchesInvolved.clear();
			std::map<int,std::set<int> > trialNeighbourList = neighbourList;

			
			std::set<int> trialState = currentState;
			MutateState(trialState, patchNums, badPatches, patchHeat, trialNeighbourList, 20, rng);
			float trialScore = EvaluateStateAll(numComponents,am, patches, trialState, badPatches,
											  manualBadRel, patchAlignmentScores, patchesInvolved,trialNeighbourList);
			float delta = trialScore - baselineScore;
			printf("Calibration sample %d: delta=%f\n", i, delta);
			if (delta < 0.0f)
				negativeDeltas.push_back(delta);
		}
		T0 = CalibrateT0(negativeDeltas, 0.5f);
	    printf("Calibrated T0 = %f (from %zu negative samples)\n", T0, negativeDeltas.size());
	}
	else
	{
		T0 = initT0;
	}
	
	// ---- Main loop ----
	float currentScore = baselineScore;
	std::set<int> bestState = currentState;
	float bestScore = currentScore;
	std::vector<float> scoreLog, currentScoreLog, stateLength;

	std::vector<CandidateResult> candidates(N);

	int acceptedCount = 0;
	
	for(int iters = 0; iters<maxIters; iters++)
	{		
		if (acceptedCount>=12)
		{
			T0=T0*0.75;
			acceptedCount=0;
		}
		
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
			std::map<int,std::set<int> > localNeighbourList = neighbourList;

			if (iters != 0)
			{
				#pragma omp critical(mutate_state)
				{
					//MutateState(localState, patchNums, badPatches, patchHeat, 15*(T/T0)*0+1, localRng);
					MutateState(localState, patchNums, badPatches, patchHeat, localNeighbourList, 15*(T/T0)+1, localRng);
				}
			}

			std::set<int> localPatchesInvolved;
			// NOTE: EvaluateState needs a variant that fills patchesInvolved
			// instead of mutating patchHeat internally, so it's safe to call
			// concurrently (concurrent reads of patchHeat are fine, concurrent
			// writes are not).
			float localScore = EvaluateStateAll(numComponents,am, patches, localState, badPatches,
			                                  manualBadRel, patchAlignmentScores, localPatchesInvolved,localNeighbourList);

			candidates[n].state = std::move(localState);
			candidates[n].score = localScore;
			candidates[n].patchesInvolved = std::move(localPatchesInvolved);
			candidates[n].neighbourList = std::move(localNeighbourList);
		}

		// We don't want to just pick the best of the N candidates or this would reduce
		// the smoothing that simulated annealing depends on - it would be the same as reducing the temperature.
		// Instead we try accepting each one in turn
		
		for(int n=0; n<N; n++)
		{
			DecayPatchHeat(patchHeat);
			UpdatePatchHeat(patchHeat, candidates[n].patchesInvolved);

			if ((iters==0 && n==0) || Accept(candidates[n].score - currentScore, T, rng))
		    {
			    printf("Accepted %d (T=%f)\n",n,T);
			    currentScore = candidates[n].score;
			    currentState = candidates[n].state;
				neighbourList = std::move(candidates[n].neighbourList);
				acceptedCount++;
		    }
			else
			{
				acceptedCount = 0;
			}
			
		    if (candidates[n].score > bestScore)
		    {
			    bestScore = candidates[n].score;
			    bestState = candidates[n].state;
		    }
		    scoreLog.push_back(candidates[n].score);
		    currentScoreLog.push_back(currentScore);
		    stateLength.push_back(candidates[n].state.size());

			printf("State size = %d, score = %f\n",
		       (int)candidates[n].state.size(), candidates[n].score);

		}
		
		if (iters%50==0)
		{
			char buf[256];
			sprintf(buf,"%s/annealState_out_%06d.csv",OUTPUT_DIR,(int)iters);
			std::ofstream os(buf);
			for(auto i : bestState)
			{
				os << i << std::endl;
			}
		}

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

	// Show the output from visit order - useful to see sizes of components.
	{
		std::set<int> effectiveBadPatches;

		std::set_union(badPatches.begin(), badPatches.end(),
					bestState.begin(), bestState.end(),
					std::inserter(effectiveBadPatches, effectiveBadPatches.begin()));

		std::vector<int> patchOrder;
		std::vector<std::pair<int,alignment>> alignmentOrder;
		std::map<int,affineTx> patchPositions;
		std::map<int,std::set<int> > neighbourList;

		MakeVisitOrder(am, patches, effectiveBadPatches, manualBadRel,
	               patchOrder, alignmentOrder, patchPositions, neighbourList, patchAlignmentScores,true);
	}
}
