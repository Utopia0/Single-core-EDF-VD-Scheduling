#include "EDFVD.h"

// Set task queue by modifying operator '<'
bool Job::operator<(const Job& other) const {
    if (schedulingDeadline != other.schedulingDeadline)
        return schedulingDeadline > other.schedulingDeadline;  // 1. Prioritize smaller schedulingDeadline

    if (useVirtualDeadline != other.useVirtualDeadline)
        return !useVirtualDeadline;  // 2. Prioritize seting VirtualDeadline as true

    if (criticalityLevel != other.criticalityLevel)
        return criticalityLevel < other.criticalityLevel;  // 3. Prioritize higher criticalityLevel

    if (originalDeadline != other.originalDeadline)
        return originalDeadline > other.originalDeadline;  // 4. Prioritize smaller originalDeadline

    if (releaseTime != other.releaseTime)
        return releaseTime > other.releaseTime;  // 5.Prioritize smaller releaseTime

    return taskID > other.taskID;  // 6. Prioritize smaller taskID
}

// Constructor function
Task::Task(int id, int level, bool periodic, int r, int p, int d, const vector<int>& wcet_values)
    : taskID(id), criticalityLevel(level), isPeriodic(periodic), releaseTime(r), period(p), deadline(d), wcet(wcet_values), taskNumber(0) {
    ComputeUtilization();  // Calculate utilization during initialization
    // PrintTaskInfo();
}


// Calculate utilization rates at different critical levels
void Task::ComputeUtilization() {
    utilization.clear();    // Clear the original data
    for (int i : wcet) {
        utilization.push_back(static_cast<double>(i) / deadline);
    }
}

// Print task information
void Task::PrintTaskInfo() {
    cout << "Task " << taskID << " (Criticality Level: " << criticalityLevel << ")\n";
    cout << "  Period: " << period << ", Deadline: " << deadline << "\n";
    cout << "  WCET: ";
    for (size_t i = 0; i < wcet.size(); ++i) {
        cout << wcet[i] << " ";
    }
    cout << "\n  Utilization: ";
    for (size_t i = 0; i < utilization.size(); ++i) {
        cout << utilization[i] << " ";
    }
    cout << "\n";
}

// Constructor function
System::System(vector<Task>& list)
    : criticalityLevel(1), tasklist(list) {
    ComputeUtilization();
    ComputeVirtualDeadline();
}

void System::ComputeUtilization() {
    u_lo_lo = 0;
    u_hi_lo = 0;
    u_hi_hi = 0;
    for (const auto& task : tasklist) {
        if (task.criticalityLevel == 1) {
            u_lo_lo = u_lo_lo + task.utilization[0];
        }
        else if (task.criticalityLevel == 2)
        {
            u_hi_lo = u_hi_lo + task.utilization[0];
            u_hi_hi = u_hi_hi + task.utilization[1];
        }
    }

    cout << "u_lo_lo = " << u_lo_lo
        << "\nu_hi_lo = " << u_hi_lo
        << "\nu_hi_hi = " << u_hi_hi << endl;

    scaleFactor = u_hi_lo / (1 - u_lo_lo);

    cout << "\nscaleFactor = " << scaleFactor << endl;
    if (scaleFactor > 1) {
        scaleFactor = 1;
        cerr << "\nWarning: scaleFactor > 1. Set scaleFactor to 1." << endl;
    }

    // double schedulabilityFactor = max(u_lo_lo + u_hi_lo, u_hi_hi);
    double schedulabilityFactor = u_lo_lo + u_hi_lo;

    cout << "\nNecessary Schedulability Factor = " << schedulabilityFactor << endl;

    if (schedulabilityFactor > 1) {
        cout << "\nThis task set may not be schedulable \n";
        // exit(1);
    }
}

// compute virtual deadline according to scaleFactor
void System::ComputeVirtualDeadline() {
    for (auto& task : tasklist) {
        if (task.criticalityLevel == 2) {
            task.virtualDeadline = scaleFactor * task.deadline;
        }
    }
}

// Constructor function
Scheduler::Scheduler(System& sys)
    : system(sys), currentTime(0), currentJob(nullptr), modeSwitchPending(false), gen(rd()) {}


// Main scheduling function
void Scheduler::EDF_VD_Schedule(int simulationDuration) {
    cout << "\n***********************************************\n" << endl;
    cout << "EDF-VD Scheduler Staring" << endl;
    cout << "\n***********************************************\n" << endl;

    for (currentTime = 0; currentTime <= simulationDuration; ++currentTime) {
        // Generate new assignments that arrive at the current time
        GenerateJobs(currentTime);
        // Check the completion status of the current jobs
        if (currentJob != nullptr) {
            currentJob->executedTime++;
            // Job complete
            if (currentJob->executedTime >= currentJob->executionTime) {
                cout << "Time " << currentTime << ": [Task " << currentJob->taskID << " Job" << currentJob->jobID
                    << "] completed, executionTime is " << currentJob->executionTime << endl;
                delete currentJob;
                currentJob = nullptr;
            }
            // Mode switch
            else if (currentJob->executedTime >= currentJob->wcet_LO && currentJob->criticalityLevel == 2 && system.criticalityLevel == 1) {
                HandleModeSwitch();
            }
            else if (!readyQueue.empty() && currentJob->schedulingDeadline > readyQueue.top().schedulingDeadline) {
                // Occurrence of preemption
                cout << "Time " << currentTime << ": [Task " << currentJob->taskID << " Job" << currentJob->jobID
                    << "] is suspended, executed time is " << currentJob->executedTime << endl;
                HandleJobPreemption(currentJob, readyQueue);
                cout << "Time " << currentTime << ": Start executing [Task";
                if (!currentJob->useVirtualDeadline) {
                    cout << currentJob->taskID << " Job" << currentJob->jobID << "] (Deadline @ "
                        << currentJob->schedulingDeadline << ")\n";
                }
                else {
                    cout << currentJob->taskID << " Job" << currentJob->jobID << "] (Vitural Deadline @ "
                        << currentJob->schedulingDeadline << ")\n";
                }
            }
        }
        // Choose a new job
        if (currentJob == nullptr && !readyQueue.empty()) {
            currentJob = new Job(readyQueue.top());
            readyQueue.pop();
            cout << "Time " << currentTime << ": Start executing [Task";
            if (!currentJob->useVirtualDeadline) {
                cout << currentJob->taskID << " Job" << currentJob->jobID << "] (Deadline @ "
                    << currentJob->schedulingDeadline << ")\n";
            }
            else {
                cout << currentJob->taskID << " Job" << currentJob->jobID << "] (Vitural Deadline @ "
                    << currentJob->schedulingDeadline << ")\n";
            }
        }
        // Check if any jobs have missed the deadline
        CheckMissedDeadlines();
    }
}

// Create job object
Job Scheduler::CreateJob(Task& task, int releaseTime) {
    // A new job is created, so taskNumber + 1
    task.taskNumber = task.taskNumber + 1;

    bool useVirtualDeadline = (task.criticalityLevel == 2 && system.criticalityLevel == 1);
    int schedulingDeadline;
    double executionTime;

    // Set schedulingDeadline (due to task level) and executionTime (randomly)
    if (useVirtualDeadline) {
        // HI level tasks in LO system
        schedulingDeadline = static_cast<int>(ceil(task.virtualDeadline));    // Round up, but prioritize scheduling during scheduling

        double lowerBound = min(0.7 * task.wcet[0], 0.5 * task.wcet[1]);
        double upperBound = task.wcet[1];
        uniform_real_distribution<> dis(lowerBound, upperBound);
        executionTime = dis(gen);
    }
    else {
        // LO level tasks , or HI level tasks in HI system
        schedulingDeadline = task.deadline;                 // For LO task, do not modify the deadline

        double lowerBound = 0.5 * task.wcet[task.criticalityLevel - 1];
        double upperBound = task.wcet[task.criticalityLevel - 1];
        uniform_real_distribution<> dis(lowerBound, upperBound);
        executionTime = dis(gen);
    }

    return {
        task.taskID,
        task.taskNumber,
        task.criticalityLevel,
        releaseTime,
        releaseTime + schedulingDeadline,  // scheduling deadline
        releaseTime + task.deadline,       // original absolute deadline
        0,  // executedTime
        executionTime,
        task.wcet[0],
        useVirtualDeadline
    };
    /*
    The definition of Job:
    int taskID;
    int jobID;                 // Job id (j) means it is the j-th job of the task
    int criticalityLevel;      // 1: LO , 2: HI
    int releaseTime;
    int schedulingDeadline;    // Use virtual deadline (HI task in LO mode)
    int originalDeadline;      // Original deadline (for restoration)
    int executedTime;          // The time this job has been executed
    double executionTime;      // The expected execution time of this job , randomly generated
    int wcet_LO;
    bool useVirtualDeadline;    // Mark whether to use virtual deadline
    */
}

// Generate Jobs
void Scheduler::GenerateJobs(int currentTime) {
    for (auto& task : system.tasklist) {
        if (task.isPeriodic && (currentTime - task.releaseTime) % task.period == 0
            || !task.isPeriodic && currentTime == task.releaseTime) {
            Job newJob = CreateJob(task, currentTime);
            readyQueue.push(newJob);
            if (newJob.useVirtualDeadline) {
                cout << "Time " << currentTime << ": New Job [Task" << newJob.taskID << " Job" << newJob.jobID << "]"
                    << " released (Virtual Deadline @ " << newJob.schedulingDeadline << ")\n";
            }
            else {
                cout << "Time " << currentTime << ": New Job [Task" << newJob.taskID << " Job" << newJob.jobID << "]"
                    << " released (Deadline @ " << newJob.schedulingDeadline << ")\n";
            }            
        }
    }
}

void Scheduler::CheckMissedDeadlines() {
    priority_queue<Job> tempQueue = readyQueue;
    while (!tempQueue.empty()) {
        const Job& job = tempQueue.top();
        if (currentTime > job.schedulingDeadline) {
            cerr << "WARNING: [Task" << job.taskID << " Job" << job.jobID << "] missed deadline at "
                << job.schedulingDeadline << endl;
            if (job.criticalityLevel == 2) {
                cerr << "\nERROR: HI level job missed deadline\n" << endl;
            }
        }
        tempQueue.pop();
    }
}

// Switch the processing mode to HI critical level
void Scheduler::HandleModeSwitch() {
    cout << "\n=== Switching to HI-Criticality Mode at Time " << currentTime << " ===\n";
    system.criticalityLevel = 2;

    // Update the deadline for HI jobs in the queue
    priority_queue<Job> newQueue;
    while (!readyQueue.empty()) {
        Job job = readyQueue.top();
        readyQueue.pop();

        if (job.criticalityLevel == 2) {
            // Restore original deadline
            job.schedulingDeadline = job.originalDeadline;
            job.useVirtualDeadline = false;
        }
        else {
            // Discard LO critical level tasks
            continue;
        }
        newQueue.push(job);
    }
    readyQueue = newQueue;
}

// Dealing with the task preemption, swapping the current job with the queue header job
void Scheduler::HandleJobPreemption(Job* currentJob, priority_queue<Job>& queue) {
    if (!currentJob || queue.empty()) {
        return;
    }

    Job highestPriorityJob = readyQueue.top();
    queue.pop();

    queue.push(*currentJob);
    *currentJob = highestPriorityJob;
}


int main() {
    // Set task set
    vector<Task> taskA = {
        Task(1, 1, true, 0, 10, 10, {3, 0}),
        Task(2, 1, true, 0, 9, 9, {4, 0}),
        Task(3, 2, true, 3, 20, 20, {3, 4})
    };

    //
    vector<Task> taskB = {
        Task(1, 1, true, 0, 10, 10, {3, 0}),
        Task(2, 1, true, 1, 12, 12, {2, 0}),
        Task(3, 2, true, 0, 15, 15, {3, 6}),
        Task(4, 2, true, 0, 20, 20, {3, 10})
    };

    vector<Task> taskC = {
        Task(1, 1, true, 0, 9, 9, {3, 0}),
        Task(2, 2, true, 1, 12, 12, {4, 6}),
        Task(3, 2, true, 0, 10, 10, {3, 5})
    };

    System systemA(taskA);
    Scheduler scheduler(systemA);


    scheduler.EDF_VD_Schedule(60);

    return 0;
}
