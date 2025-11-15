#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <mpi.h>

using namespace std;

#define ELECTION_TAG 0
#define ELECTED_TAG 1

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const int num_processes = 6;
    if (size != num_processes) {
        if (rank == 0) {
            cerr << "Error: This program must be run with exactly "
                 << num_processes << " processes." << endl;
        }
        MPI_Finalize();
        return 1;
    }

    // Rank :   0   1   2   3   4   5
    // ID   :   3  32   5  80   6  12
    vector<int> ids = {3, 32, 5, 80, 6, 12};

    int my_id = ids[rank];
    int neighbor_rank = (rank + 1) % size;
    int leader_id = -1;
    bool is_participant = false;
    bool is_active = true;

    MPI_Barrier(MPI_COMM_WORLD); 
    if (rank == 0) {
        cout << "[Rank " << rank << ", ID " << my_id << "] Initiates the election." << endl;
        MPI_Send(&my_id, 1, MPI_INT, neighbor_rank, ELECTION_TAG, MPI_COMM_WORLD);
        is_participant = true;
    }

    while (is_active) {
        int received_id;
        MPI_Status status;

        MPI_Recv(&received_id, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == ELECTION_TAG) {
            // Case 1: Received an election message
            if (received_id > my_id) {
                cout << "[Rank " << rank << ", ID " << my_id
                     << "] → Forwarding stronger candidate ID " << received_id << "." << endl;
                MPI_Send(&received_id, 1, MPI_INT, neighbor_rank, ELECTION_TAG, MPI_COMM_WORLD);
                is_participant = true;
            }

            else if (received_id < my_id && !is_participant) {
                cout << "[Rank " << rank << ", ID " << my_id
                     << "] ← Absorbed weaker ID " << received_id
                     << ", sending my own ID " << my_id << "as the stronger candidate." << endl;
                MPI_Send(&my_id, 1, MPI_INT, neighbor_rank, ELECTION_TAG, MPI_COMM_WORLD);
                is_participant = true;
            }

            else if (received_id == my_id) {
                cout << "[Rank " << rank << ", ID " << my_id
                     << "] I AM THE LEADER!" << endl;
                leader_id = my_id;
                is_active = false;
                MPI_Send(&my_id, 1, MPI_INT, neighbor_rank, ELECTED_TAG, MPI_COMM_WORLD);
            }
        }

        else if (status.MPI_TAG == ELECTED_TAG) {
            // Case 2: Received a leader announcement
            leader_id = received_id;
            is_active = false;

            cout << "[Rank " << rank << ", ID " << my_id
                 << "] Learned that the leader is ID " << leader_id << "." << endl;

            if (my_id != leader_id) {
                MPI_Send(&leader_id, 1, MPI_INT, neighbor_rank, ELECTED_TAG, MPI_COMM_WORLD);
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        cout << "\n-------------------------------------------------\n";
        cout << "Election Complete. Final Results:\n";
        cout << "-------------------------------------------------\n";
    }
    MPI_Barrier(MPI_COMM_WORLD);

    for (int i = 0; i < size; ++i) {
        if (rank == i) {
            cout << "   [Rank " << rank << "] My ID is " << my_id
                 << ". The elected leader is ID " << leader_id << "." << endl;
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}
