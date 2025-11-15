#include <mpi.h>
#include <bits/stdc++.h> 
#include <unistd.h>   

using namespace std;

// update logical clock fxn
void update_clock(int& local_clock, int received_clock) {
    if (received_clock == -1) {
        local_clock++;
        return;
    }
    local_clock = max(local_clock, received_clock) + 1;
}

// print logical clock fxn
void print_logical_clock(int rank, int clock, const string& message) {
    cout << "[Rank " << rank << "] " << message << " LC: " << clock << endl;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (world_size < 2) {
        if (world_rank == 0) {
            cerr << "This program requires at least 2 processes." << endl;
        }
        MPI_Finalize();
        return 1;
    }

    int logical_clock = 0;
    srand(time(NULL) + world_rank);

    MPI_Barrier(MPI_COMM_WORLD);
    if (world_rank == 0) {
        cout << "--- Lamport Logical Clock Simulation Starting ---" << endl;
    }
    MPI_Barrier(MPI_COMM_WORLD);

    const int NUM_ITERATIONS = 10;
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        usleep(10000 * (1 + (rand() % 50)));

        int flag = 0;
        MPI_Status status;
        MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);

        if (flag) {
            int received_clock;
            int source_rank = status.MPI_SOURCE;
            MPI_Recv(&received_clock, 1, MPI_INT, source_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            cout << "[Rank " << world_rank << "] Received clock value " << received_clock << " from Rank " << source_rank << "." << endl;
            update_clock(logical_clock, received_clock);
            print_logical_clock(world_rank, logical_clock, "Updated after receive.");
        }

        int action_choice = rand() % 3; 

        if (action_choice == 0) { // Send event
            update_clock(logical_clock,-1); 
            int dest_rank;
            do {
                dest_rank = rand() % world_size;
            } while (dest_rank == world_rank);
            MPI_Request send_request;
            MPI_Isend(&logical_clock, 1, MPI_INT, dest_rank, 0, MPI_COMM_WORLD, &send_request);
            print_logical_clock(world_rank, logical_clock, "Sent to Rank " + to_string(dest_rank) + ".");

        } 
        else { // Internal event
            update_clock(logical_clock,-1); 
            print_logical_clock(world_rank, logical_clock, "Internal event.       ");
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    if (world_rank == 0) {
        cout << "\n--- Simulation Finished ---" << endl;
    }
    MPI_Barrier(MPI_COMM_WORLD);

    for (int rank = 0; rank < world_size; ++rank) {
        if (world_rank == rank) {
            print_logical_clock(world_rank, logical_clock, "Final State.          ");
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}