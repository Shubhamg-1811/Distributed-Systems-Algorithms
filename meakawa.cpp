#include <mpi.h>
#include <iostream>
#include <vector>
#include <queue> 
#include <utility>
#include <algorithm>
#include <chrono>
#include <thread>
#include <string> 

using namespace std;

// Message tags
#define REQ_TAG       10
#define YES_TAG       11
#define INQUIRE_TAG   12
#define RELINQ_TAG    13
#define RELEASE_TAG   14

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const int NUM_PROCESSES = 6;
    if (size != NUM_PROCESSES) {
        if (rank == 0)
            cerr << "Run with exactly " << NUM_PROCESSES << " processes.\n";
        MPI_Finalize();
        return 1;
    }

    // voting districts
    vector<vector<int>> S = {
        {0,1,2,3}, {0,1,2,4}, {0,1,2,5}, {0,3,4,5}, {1,3,4,5}, {2,3,4,5}
    };
    vector<int> mySet = S[rank];

    int Ts = 0;                          
    int Yes_votes = 0;                  
    bool WantCS = false;               
    bool HaveVoted = false;             
    int Candidate = -1;                 
    int Candidate_Ts = 0;
    bool HaveInquired = false;           

    priority_queue<pair<int,int>, vector<pair<int,int>>, greater<pair<int,int>>> WaitingQ;

    auto sendInt = [&](int dest, int tag, int value){
        MPI_Send(&value, 1, MPI_INT, dest, tag, MPI_COMM_WORLD);
    };

    auto log = [&](const string &msg){
         cout << "[Rank " << rank << "] " << msg << endl;
    };
    
    auto log_nb = [&](const string &msg){
         cout << "[Rank " << rank << "] " << msg << endl;
    };


    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) cout << "=== Starting Maekawa DME simulation (Corrected) ===\n";

    MPI_Barrier(MPI_COMM_WORLD);
    std::this_thread::sleep_for(std::chrono::milliseconds(100 * (rank % 2))); 

    bool initiator = (rank == 1) || (rank == 5);

    if (initiator) {
        WantCS = true;
        Ts++;                         
        Yes_votes = 1;                

        HaveVoted = true;
        Candidate = rank;
        Candidate_Ts = Ts;

        log_nb("Wants CS. Broadcasting REQUEST to voting set (ts=" + to_string(Ts) + ")");

        // send REQ(ts, pid) 
        for (int member : mySet) {
            if (member == rank) continue; 
            sendInt(member, REQ_TAG, Ts);
            sendInt(member, REQ_TAG, rank);
        }
    }

    bool done = false;
    bool inCS = false;
    int expectedYes = (int)mySet.size();

    auto start = chrono::steady_clock::now();
    const int TIMEOUT_MS = 15000;

    while (!done) {
        MPI_Status status;
        int flag = 0;
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
        
        if (flag) {
            int src = status.MPI_SOURCE;
            int tag = status.MPI_TAG;

            if (tag == REQ_TAG) {
                int recv_ts, recv_pid;
                MPI_Recv(&recv_ts, 1, MPI_INT, src, REQ_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&recv_pid, 1, MPI_INT, src, REQ_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                Ts = max(Ts, recv_ts) + 1;
                log_nb("Received REQUEST from rank " + to_string(recv_pid) + " (ts=" + to_string(recv_ts) + ")");

                if (!HaveVoted) {
                    HaveVoted = true;
                    Candidate = recv_pid;
                    Candidate_Ts = recv_ts;
                    HaveInquired = false; 
                    log_nb(" -> Granted YES to " + to_string(recv_pid));
                    sendInt(recv_pid, YES_TAG, rank);
                } 
                else {
                    WaitingQ.push({recv_ts, recv_pid});
                    log_nb(" -> Deferred request from " + to_string(recv_pid) + " (candidate=" + to_string(Candidate) + ", c_ts=" + to_string(Candidate_Ts) + ")");
                    
                    bool higher_priority = (recv_ts < Candidate_Ts) || (recv_ts == Candidate_Ts && recv_pid < Candidate);
                    if (higher_priority && !HaveInquired) {
                        log_nb(" -> New request has higher priority. Sending INQUIRE to current Candidate " + to_string(Candidate));
                        sendInt(Candidate, INQUIRE_TAG, rank);
                        HaveInquired = true;
                    }
                }
            }
            else if (tag == YES_TAG) {
                int sender_rank;
                MPI_Recv(&sender_rank, 1, MPI_INT, src, YES_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                Ts = Ts + 1;
                if(WantCS) { 
                    Yes_votes++;
                    log_nb("Received YES from rank " + to_string(sender_rank) + " -> Yes_votes=" + to_string(Yes_votes));
                } 
                else {
                    log_nb("Received a stray YES from " + to_string(sender_rank) + ", ignoring.");
                }
            }
            else if (tag == INQUIRE_TAG) {
                int inquirer;
                MPI_Recv(&inquirer, 1, MPI_INT, src, INQUIRE_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                Ts = Ts + 1;
                log_nb("Received INQUIRE from rank " + to_string(inquirer));

                if (WantCS && !inCS) {
                    // Send RELINQUISH to inquirer (the voter)
                    log_nb(" -> Still waiting for CS. Sending RELINQUISH to " + to_string(inquirer));
                    sendInt(inquirer, RELINQ_TAG, rank);
                    Yes_votes = max(0, Yes_votes - 1);
                } 
                else {
                    log_nb(" -> Not relinquishing (inCS=" + string(inCS ? "true":"false") + ", WantCS=" + string(WantCS ? "true":"false") + ")");
                }
            }
            else if (tag == RELINQ_TAG) {
                int rel_sender; 
                MPI_Recv(&rel_sender, 1, MPI_INT, src, RELINQ_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                Ts = Ts + 1;
                log_nb("Received RELINQUISH from " + to_string(rel_sender));

                if (rel_sender != Candidate) {
                     log_nb(" -> WARNING: Received RELINQUISH from " + to_string(rel_sender) + " but my candidate was " + to_string(Candidate));
                }

                WaitingQ.push({Candidate_Ts, Candidate});
                auto next = WaitingQ.top();
                WaitingQ.pop();
                
                if (next.first != -1) {
                    int chosen_pid = next.second;
                    Candidate = chosen_pid;
                    Candidate_Ts = next.first;
                    HaveInquired = false;
                    HaveVoted = true;    
                    
                    log_nb(" -> Granting YES to " + to_string(chosen_pid) + " after RELINQUISH");
                    sendInt(chosen_pid, YES_TAG, rank);
                } else {
                    HaveVoted = false;
                    Candidate = -1;
                    Candidate_Ts = 0;
                    HaveInquired = false;
                    log_nb(" -> No waiting requests; vote freed (UNEXPECTED after RELINQUISH)");
                }
            }
            else if (tag == RELEASE_TAG) {
                int rel_pid;
                MPI_Recv(&rel_pid, 1, MPI_INT, src, RELEASE_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                Ts = Ts + 1;
                log_nb("Received RELEASE from " + to_string(rel_pid));

                if (rel_pid != Candidate) {
                    log_nb(" -> WARNING: Received RELEASE from " + to_string(rel_pid) + " but my candidate was " + to_string(Candidate));
                }

                HaveVoted = false;
                Candidate = -1;
                Candidate_Ts = 0;
                HaveInquired = false;

                if (!WaitingQ.empty()) {
                    auto nxt = WaitingQ.top();
                    WaitingQ.pop();
                    int chosen = nxt.second;
                    Candidate = chosen;
                    Candidate_Ts = nxt.first;
                    HaveVoted = true; 
                    
                    log_nb(" -> Granting YES to " + to_string(chosen) + " due to RELEASE");
                    sendInt(chosen, YES_TAG, rank);
                } 
                else {
                    log_nb(" -> No waiting requests after RELEASE; vote freed");
                }
            }
            
        } 

        // Check if enter CS
        if (initiator && WantCS && !inCS) {
            if (Yes_votes >= expectedYes) {
                // === ENTER CS ===
                inCS = true;
                log("=== ENTERING CRITICAL SECTION (ts=" + to_string(Ts) + ") ===");
                std::this_thread::sleep_for(std::chrono::milliseconds(500 + 50 * rank));
                log("=== LEAVING CRITICAL SECTION ===");
                // === EXIT CS ===

                // Sending RELEASE 
                WantCS = false;
                inCS = false;
                Yes_votes = 0;
                
                for (int member : mySet) {
                    if (member == rank) continue;
                    sendInt(member, RELEASE_TAG, rank);
                }
                
                log_nb(" -> Releasing my own vote.");
                HaveVoted = false;
                Candidate = -1;
                Candidate_Ts = 0;
                HaveInquired = false;

                if (!WaitingQ.empty()) {
                    auto nxt = WaitingQ.top();
                    WaitingQ.pop();
                    int chosen = nxt.second;
                    Candidate = chosen;
                    Candidate_Ts = nxt.first;
                    HaveVoted = true; 
                    
                    log_nb(" -> Granting YES to " + to_string(chosen) + " (from self-release)");
                    sendInt(chosen, YES_TAG, rank);
                } 
                else {
                    log_nb(" -> My vote is now free, nobody is waiting.");
                }
                done = true;
            }
        }
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - start).count();
        if (!initiator && elapsed > (TIMEOUT_MS * 0.75) && WaitingQ.empty() && !HaveVoted) {
             done = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) cout << "=== Simulation finished (Maekawa) ===\n";

    MPI_Finalize();
    return 0;
}