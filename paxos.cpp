#include <mpi.h>
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <chrono>
#include <thread>
#include <string>

using namespace std;

#define PREPARE_TAG        10 // <prepare, n>
#define PROMISE_TAG        11 // <promise, n, (na, va)> or <promise, n, null>
#define PREPARE_FAILED_TAG 12 // <prepare-failed>
#define ACCEPT_TAG         13 // <accept, (n, v)>
#define ACCEPTED_TAG       14 
#define DECIDE_TAG         15 

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 3) {
        if (rank == 0) cerr << "Run with at least 3 processes.\n";
        MPI_Finalize();
        return 1;
    }

    int nh = -1; 
    int na = -1;
    int va = -1;

    bool is_proposer = (rank == 0 || rank == 1 || rank == 2); 
    
    int n = rank; 
    int round_count = 1;
    int v = 1000 + rank; 

    int promises_received = 0;
    int max_na_seen = -1; 
    
    bool proposal_active = false;
    bool proposal_phase2 = false; 

    map<int, int> vote_counts; // Map <ProposalID, Count>
    bool consensus_reached = false;
    int quorum = (size / 2) + 1;

    auto sendPacket = [&](int dest, int tag, int _n, int _v, int _na){
        int buf[3] = {_n, _v, _na};
        MPI_Send(buf, 3, MPI_INT, dest, tag, MPI_COMM_WORLD);
    };

    auto log_nb = [&](const string &msg){
         cout << "[Rank " << rank << "] " << msg << endl;
    };

    MPI_Barrier(MPI_COMM_WORLD);
    std::this_thread::sleep_for(std::chrono::milliseconds(100 * (rank % 3))); 

    auto increment_n = [&]() {
        n = (round_count++ * size) + rank;
    };

   // Phase 1: PREPARE
    if (is_proposer) {
        increment_n(); 
        proposal_active = true;
        log_nb("Proposer: Sending <prepare, " + to_string(n) + ">");
        for(int i=0; i<size; i++) sendPacket(i, PREPARE_TAG, n, -1, -1);
    }

    bool done = false;
    auto start_sim = chrono::steady_clock::now();

    while (!done) {
        MPI_Status status;
        int flag = 0;
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);

        if (flag) {
            int src = status.MPI_SOURCE;
            int tag = status.MPI_TAG;
            int buf[3];
            MPI_Recv(buf, 3, MPI_INT, src, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            int recv_n = buf[0];
            int recv_v = buf[1]; 
            int recv_na = buf[2]; 

            if (tag == PREPARE_TAG) {
                if (recv_n > nh) {
                    nh = recv_n;
                    sendPacket(src, PROMISE_TAG, recv_n, va, na);
                    log_nb("Acceptor: Promised n=" + to_string(recv_n) + " (Previous na=" + to_string(na) + ")");
                } 
                else {
                    sendPacket(src, PREPARE_FAILED_TAG, recv_n, -1, -1);
                    log_nb("Acceptor: Rejected prepare n=" + to_string(recv_n) + " (Current nh=" + to_string(nh) + ")");
                }
            }

            else if (tag == PROMISE_TAG) {
                if (is_proposer && proposal_active && !proposal_phase2 && recv_n == n) {
                    promises_received++;
                    
                    if (recv_na > max_na_seen) {
                        max_na_seen = recv_na;
                        v = recv_v;
                        log_nb("Proposer: Observed higher na=" + to_string(recv_na) + ". Updating v to " + to_string(v));
                    }

                    if (promises_received >= quorum) {
                        proposal_phase2 = true;                        
                        log_nb("Proposer: Majority Reached. Sending <accept, " + to_string(n) + ", " + to_string(v) + ">");
                        for(int i=0; i<size; i++) sendPacket(i, ACCEPT_TAG, n, v, -1);
                    }
                }
            }
            else if (tag == PREPARE_FAILED_TAG) {
                if (is_proposer && recv_n == n) {
                    proposal_active = false; 
                }
            }

       // PHASE 2: ACCEPT
            else if (tag == ACCEPT_TAG) {
                if (recv_n >= nh) {
                    na = recv_n;
                    nh = recv_n; 
                    va = recv_v;
                    
                    log_nb("Acceptor: Accepted <n=" + to_string(na) + ", v=" + to_string(va) + ">");
                    
                    for(int i=0; i<size; i++) {
                         sendPacket(i, ACCEPTED_TAG, na, va, -1);
                    }
                } 
                else log_nb("Acceptor: Ignored Accept n=" + to_string(recv_n) + " because nh=" + to_string(nh));
            }

       // PHASE 3: LEARN
            else if (tag == ACCEPTED_TAG) {
                vote_counts[recv_n]++;
                
                if (vote_counts[recv_n] >= quorum && !consensus_reached) {
                    consensus_reached = true;
                    log_nb("=== CONSENSUS REACHED: Value " + to_string(recv_v) + " (Proposal n=" + to_string(recv_n) + ") ===");
                    
                    for(int i=0; i<size; i++) sendPacket(i, DECIDE_TAG, recv_n, recv_v, -1);
                    done = true;
                }
            }
            else if (tag == DECIDE_TAG) {
                if(!done) {
                    log_nb("Decide received. Value: " + to_string(recv_v));
                    done = true;
                }
            }
        }
        
    }

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return 0;
}