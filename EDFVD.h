#pragma once

#ifndef EDFVD_H
#define EDFVD_H

#include <iostream>
#include <vector>
#include <queue>
#include <random>
#include <cmath>

using namespace std;

struct Job {
    int taskID;
    int jobID;
    int criticalityLevel;
    int releaseTime;
    int schedulingDeadline;
    int originalDeadline;
    int executedTime;
    double executionTime;
    int wcet_LO;
    bool useVirtualDeadline;

    bool operator<(const Job& other) const;
};

class Task {
public:
    int taskID;
    int criticalityLevel;      // 1: LO , 2: HI
    bool isPeriodic;            // True: periodic task, False: sporadic task or single task
    int releaseTime;
    int period;
    int deadline;
    vector<int> wcet;

    int taskNumber;            // To calculate how many jobs has been created
    vector<double> utilization;
    double virtualDeadline;

    Task(int id, int level, bool periodic, int r, int p, int d, const vector<int>& wcet_values);

private:
    void ComputeUtilization();
    void PrintTaskInfo();
};

class System {
public:
    int criticalityLevel;   // 1: LO , 2: HI
    vector<Task>& tasklist;

    double u_lo_lo;         // The total utilization of all LO tasks in the LO system 
    double u_hi_lo;         // The total utilization of all HI tasks in the LO system 
    double u_hi_hi;         // The total utilization of all HI tasks in the HI system 
    double scaleFactor;     // Calculated by EDF-VD algorithm, aim to reduce HI level tasks' deadline

    System(vector<Task>& list);

private:
    void ComputeUtilization();
    void ComputeVirtualDeadline();
};

class Scheduler {
private:
    System& system;
    priority_queue<Job> readyQueue;
    int currentTime;
    Job* currentJob;
    bool modeSwitchPending;
    random_device rd;
    mt19937 gen;

public:
    Scheduler(System& sys);
    void EDF_VD_Schedule(int simulationDuration);

private:
    Job CreateJob(Task& task, int releaseTime);
    void GenerateJobs(int currentTime);
    void CheckMissedDeadlines();
    void HandleModeSwitch();
    void HandleJobPreemption(Job* currentJob, priority_queue<Job>& queue);
};

#endif
