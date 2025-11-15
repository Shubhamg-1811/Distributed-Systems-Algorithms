#include <bits/stdc++.h>
#include <mpi.h>
#include <unistd.h>   

using namespace std;

#define M_C_TAG 0 
#define M_P_TAG 1
#define M_R_TAG 2 

#define MAX_PROCESSES 10
#define MAX_NEIGHBOURS 10

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const int num_processes = 6; 
    if (size != num_processes) {
        if (rank == 0) {
            cerr << "Error: This program must be run with exactly " << num_processes << " processes.\n";
            cerr << "Example: mpirun -np " << num_processes << " ./your_executable\n";
        }
        MPI_Finalize();
        return 1;
    }
    
    vector<vector<int>>graph(num_processes, vector<int>(num_processes, 0));
    graph[0][1] = graph[1][0] = 1;
    graph[0][3] = graph[3][0] = 1;
    graph[1][2] = graph[2][1] = 1;
    graph[1][3] = graph[3][1] = 1;
    graph[1][4] = graph[4][1] = 1;
    graph[3][4] = graph[4][3] = 1;
    graph[4][5] = graph[5][4] = 1;

    int parent = -1;
    vector<int> children;
    vector<int> neighbours;
    int noResponseRemaining = 0;
    bool has_parent = false;

    for (int i = 0; i < num_processes; ++i) {
        if (graph[rank][i]) {
            neighbours.push_back(i);
        }
    }

    const int root_id = 0;
    MPI_Request send_request;
    MPI_Barrier(MPI_COMM_WORLD);

    srand(time(NULL) + rank);
    sleep(rand() % 3);

    if (rank == root_id) {
        parent = root_id;
        has_parent = true;
        noResponseRemaining = neighbours.size();
        cout << "[Rank " << rank << " ROOT] Sending child proposals to " 
             << neighbours.size() << " neighbours.\n";
        for (int nb : neighbours) {
            MPI_Isend(NULL, 0, MPI_INT, nb, M_C_TAG, MPI_COMM_WORLD, &send_request);
        }
        if (noResponseRemaining == 0) {
            cout << "[Rank " << rank << " ROOT] is isolated and has no neighbours.\n";
        }
    }

    while (!has_parent || noResponseRemaining > 0) {
        MPI_Status status;
        MPI_Recv(NULL, 0, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        int sender_rank = status.MPI_SOURCE;
        int msg_tag = status.MPI_TAG;
        switch (msg_tag) {
            case M_C_TAG: { // Child Proposal
                if (!has_parent) {
                    parent = sender_rank;
                    has_parent = true;
                    cout << "[Rank " << rank << "] Accepted P" << parent << " as parent.\n";

                    MPI_Isend(NULL, 0, MPI_INT, parent, M_P_TAG, MPI_COMM_WORLD, &send_request);

                    for (int nb : neighbours) {
                        if (nb != parent) {
                            MPI_Isend(NULL, 0, MPI_INT, nb, M_C_TAG, MPI_COMM_WORLD, &send_request);
                            noResponseRemaining++;
                        }
                    }

                    if (noResponseRemaining == 0) {
                        cout << "[Rank " << rank << "] is a LEAF node.\n";
                    }
                } 
                else {
                    // Already has a parent → reject
                    cout << "[Rank " << rank << "] Already has parent P" << parent 
                         << ". Rejecting P" << sender_rank << ".\n";
                    MPI_Isend(NULL, 0, MPI_INT, sender_rank, M_R_TAG, MPI_COMM_WORLD, &send_request);
                }
                break;
            }
            case M_P_TAG: { // Parent Acceptance
                children.push_back(sender_rank);
                noResponseRemaining--;
                cout << "[Rank " << rank << "] Acknowledged P" << sender_rank 
                     << " as a child. (" << noResponseRemaining << " responses left)\n";
                break;
            }
            case M_R_TAG: { // Rejection
                noResponseRemaining--;
                cout << "[Rank " << rank << "] Received rejection from P" 
                     << sender_rank << ". (" << noResponseRemaining << " responses left)\n";
                break;
            }
        }
        sleep(rand() % 2);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    for (int i = 0; i < size; ++i) {
        if (rank == i) {
            if (rank == root_id) {
                cout << "   [Rank " << rank << " ROOT] ";
            } 
            else {
                cout << "   [Rank " << rank << "] Parent: P" << parent << ". ";
            }

            if (!children.empty()) {
                cout << "Children: ";
                for (int child : children) cout << "P" << child << " ";
            } 
            else {
                cout << "Children: None.";
            }
            cout << "\n";
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}
