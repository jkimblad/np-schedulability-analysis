#ifndef IIP_HPP
#define IIP_HPP

// IIP = Idle-time Insertion Policy
// Fork work-conserving algorithms an IIP is invoked whenever the system may
// commence execution of a job to determine wether the highest priority job is
// to be scheduled or not (work conserved).

namespace NP {

	namespace Uniproc {

		template<class Time> class Null_IIP
		{
			public:

			typedef Schedule_state<Time> State;
			typedef State_space<Time, Null_IIP> Space;
			typedef typename State_space<Time, Null_IIP>::Workload Jobs;

			typedef Job_set Scheduled;

			static const bool can_block = false;

			Null_IIP(const Space &space, const Jobs &jobs) {}

			Time latest_start(const Job<Time>& j, Time t, const Scheduled& as)
			{
				return Time_model::constants<Time>::infinity();
			}
		};

		// AER IIP
		template<class Time> class AER_IIP
		{
			public:

			typedef Schedule_state<Time> State;
			typedef State_space<Time, AER_IIP> Space;
			//Workload is a typedef of NP::Job<>::Job_set
			//Job_set is a typedef of std::Vector<Job<>>
			typedef typename State_space<Time, AER_IIP>::Workload Jobs;

			//Job_set is a typedef of class NP::Index_set
			typedef Job_set Scheduled;

			static const bool can_block = true;

			//constructor
			AER_IIP(const Space &space, const Jobs &jobs) 
			: space(space)
			, jobs(jobs)
			{}
			
			Time latest_start(const Job<Time>& j, Time t, const Scheduled& as)
			{
				//
                                //We are given a job j, a time t, and an
                                //index-set as containing all previously
                                //scheduled jobs.
				//We want to find out :
				//  1) Is the job an A or R
				//  2) Are there cores available?
				//	-Count amount of A and R jobs in as,
				//	the difference will be the amount of
				//	cores being used.
				//  3) If job is R
				//	-is A finished?
				//	    -A will always be finished, since
				//	    we are constraining its deadline to
				//	    be before the release of R
				//
                                
				int free_cores = available_cores(as, t);

				//Debug messages
				DM2("-----------------\n");
				DM2("as: " <<as <<"\n");
				DM2("Free cores: " <<free_cores <<"\n");
				DM2("t: " <<t <<"\n");


				//Check if R-phase, which means job is already
				//scheduled to a core.
				if(is_restitution_phase(j)){
					DM2("job_id: " <<j.get_id() <<" is schedulable at t: " <<t <<"\n");
					return Time_model::constants<Time>::infinity();

				} else {
					//Job is A-phase and needs to be
					//assigned a core

					//We can schedule the job now
					if(free_cores > 0){
						DM2("job_id: " <<j.get_id() <<" is schedulable at t: " <<t <<"\n");
						return Time_model::constants<Time>::infinity();
					} else {
						//No cores are available right, job is not IIP eligible at this time
						return 0;
					}
				}
			}


			private:

                        //Calculate amount of busy cores by finding the
                        //difference between scheduled A and R jobs.
			int busy_cores(const Scheduled& as, Time t)
			{

				//
				//"as" contains a bool vector, where each
				//element represents if the job with the same
				//index has been scheduled or not
				//
				
				int sum = 0; 

                                //Iterate the job vector and check if each job
                                //is scheduled or not
                                for(unsigned int i = 0; i < jobs.size() ; i++){

					//Debug messages
					DM3("-----------------\n");
					DM3("i: " <<i <<"   | job_id: " <<jobs[i].get_id() <<"\n");
					DM3("t: " <<t <<"\n");
					
					//Job is scheduled and is R-phase
					if(as.contains(i) && is_restitution_phase(jobs[i])){

						//Debug
						DM3("cmin: " <<jobs[i].maximal_cost() <<"\n");
						DM3("min: " <<space.rta.find(&jobs[i])->second.min() <<"\n");
						DM3("max: " <<space.rta.find(&jobs[i])->second.max() <<"\n");

						//During scheduling decisions,
						//jobs have always finished
						//execution. Thus the R-phase,
						//if scheduled, is guaranteed
						//to have finished at t.
						sum--;

					} 
					//Job is scheduled and is A-phase
					else if(as.contains(i) && is_acquisition_phase(jobs[i])){
						DM3("cmin: " <<jobs[i].maximal_cost() <<"\n");
						DM3("min: " <<space.rta.find(&jobs[i])->second.min() <<"\n");
						DM3("max: " <<space.rta.find(&jobs[i])->second.max() <<"\n");
						sum++;
					}
					
					DM3("-----------------\n");
				}

				return sum;
			}

			//Return amount of cores that are available for
			//scheduling
			unsigned int available_cores(const Scheduled& as,
					Time t)
			{
				return total_cores() - busy_cores(as, t);
			}

			//Check if a job is restitution phase by looking if its
			//ID is even
			bool is_restitution_phase(const Job<Time>& j) 
			{
				return !is_acquisition_phase(j);
			}

			//Check if a job is restitution phase by looking if its
			//ID is odd
			bool is_acquisition_phase(const Job<Time>& j)
			{
				return j.get_id() % 2;
			}

			//Return total amount of cores that are available for
			//scheduling jobs onto
			unsigned int total_cores()
			{
				return space.num_cores;
			}

			const Space &space;
			const Jobs &jobs;

                };

		template<class Time> class Precatious_RM_IIP
		{
			public:

			typedef Schedule_state<Time> State;
			typedef State_space<Time, Precatious_RM_IIP> Space;
			typedef typename State_space<Time, Precatious_RM_IIP>::Workload Jobs;

			typedef Job_set Scheduled;

			static const bool can_block = true;

			Precatious_RM_IIP(const Space &space, const Jobs &jobs)
			: space(space), max_priority(highest_prio(jobs))
			{
				for (const Job<Time>& j : jobs)
					if (j.get_priority() == max_priority)
						hp_jobs.insert({j.latest_arrival(), &j});
				DM("IIP max priority = " << max_priority);
			}

			Time latest_start(const Job<Time>& j, Time t, const Scheduled& as)
			{
				DM("IIP P-RM for " << j << ": ");

				// Never block maximum-priority jobs
				if (j.get_priority() == max_priority) {
					DM("Self." << std::endl);
					return Time_model::constants<Time>::infinity();
				}

				for (auto it = hp_jobs.upper_bound(t); it != hp_jobs.end(); it++) {
					const Job<Time>& h = *it->second;
					if (space.incomplete(as, h)) {
						Time latest = h.get_deadline()
						              - h.maximal_cost()
						              - j.maximal_cost();

						DM("latest=" << latest << " " << h << std::endl);
						return latest;
					}
				}

				DM("None." << std::endl);

				// If we didn't find anything relevant, then there is no reason
				// to block this job.
				return Time_model::constants<Time>::infinity();
			}

			private:

			const Space &space;
			const Time max_priority;
			std::multimap<Time, const Job<Time>*> hp_jobs;

			static Time highest_prio(const Jobs &jobs)
			{
				Time prio = Time_model::constants<Time>::infinity();
				for (auto j : jobs)
					prio = std::min(prio, j.get_priority());
				return prio;
			}

		};

		template<class Time> class Critical_window_IIP
		{
			public:

			typedef Schedule_state<Time> State;
			typedef State_space<Time, Critical_window_IIP> Space;
			typedef typename State_space<Time, Critical_window_IIP>::Workload Jobs;

			typedef Job_set Scheduled;

			static const bool can_block = true;

			Critical_window_IIP(const Space &space, const Jobs &jobs)
			: space(space)
			, max_cost(maximal_cost(jobs))
			, n_tasks(count_tasks(jobs))
			{
			}

			Time latest_start(const Job<Time>& j, Time t, const Scheduled& as)
			{
				DM("IIP CW for " << j << ": ");
				auto ijs = influencing_jobs(j, t, as);
				Time latest = Time_model::constants<Time>::infinity();
				// travers from job with latest to job with earliest deadline
				for (auto it  = ijs.rbegin(); it != ijs.rend(); it++)
					latest = std::min(latest, (*it)->get_deadline())
					         - (*it)->maximal_cost();
				DM("latest=" << latest << " " << std::endl);
				return latest - j.maximal_cost();
			}

			private:

			const Space &space;
			const Time max_cost;
			const std::size_t n_tasks;

			static Time maximal_cost(const Jobs &jobs)
			{
				Time cmax = 0;
				for (auto j : jobs)
					cmax = std::max(cmax, j.maximal_cost());
				return cmax;
			}

			static std::size_t count_tasks(const Jobs &jobs)
			{
				std::unordered_set<unsigned long> all_tasks;
				for (const Job<Time>& j : jobs)
						all_tasks.insert(j.get_task_id());
				return all_tasks.size();
			}

			std::vector<const Job<Time>*> influencing_jobs(
				const Job<Time>& j_i,
				Time at,
				const Scheduled& already_scheduled)
			{
				// influencing jobs
				std::unordered_map<unsigned long, const Job<Time>*> ijs;

				// first, check everything that's already pending at time t
				// is accounted for
				for (const Job<Time>& j : space.jobs_by_win.lookup(at)) {
					auto tid = j.get_task_id();
					if (j.scheduling_window().contains(at)
					    && j_i.get_task_id() != tid
					    && space.incomplete(already_scheduled, j)
					    && (ijs.find(tid) == ijs.end()
					        || ijs[tid]->earliest_arrival()
					           > j.earliest_arrival())) {
						ijs[tid] = &j;
					}
				}

				// How far do we need to look into future releases?
				Time latest_deadline = 0;
				for (auto ij : ijs)
					latest_deadline = std::max(latest_deadline,
					                           ij.second->get_deadline());

				// second, go looking for later releases, if we are still
				// missing tasks

				for (auto it = space.jobs_by_earliest_arrival.upper_bound(at);
				     ijs.size() < n_tasks - 1
					   && it != space.jobs_by_earliest_arrival.end();
					 it++) {
					const Job<Time>& j = *it->second;
					auto tid = j.get_task_id();

					// future jobs should still be pending...
					assert(space.incomplete(already_scheduled, j));

					if (ijs.find(tid) == ijs.end()) {
						ijs[tid] = &j;
					 	latest_deadline = std::max(latest_deadline,
					 	                           j.get_deadline());
					}

					// can we stop searching already?
					if (latest_deadline + max_cost < it->first) {
						// we have reached the horizon --- whatever comes now
						// cannot influence the latest start time anymore
					 	break;
					}
				}

				std::vector<const Job<Time>*> i_jobs_by_release_time;

				for (auto ij : ijs)
					i_jobs_by_release_time.push_back(ij.second);

				auto by_deadline = [](const Job<Time>* a, const Job<Time>* b)
				{
					return a->get_deadline() < b->get_deadline();
				};

				std::sort(i_jobs_by_release_time.begin(),
				          i_jobs_by_release_time.end(),
				          by_deadline);

				return i_jobs_by_release_time;
			}

		};

	}
}

#endif
