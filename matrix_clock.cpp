#include <mpi.h>
#include <bits/stdc++.h>
#include <unistd.h>

using namespace std;

// print matrix clock fxn
void print_matrix_clock(const vector<vector<int>>& clock, int size, int rank, const string& message) {
    cout << "[Rank " << rank << "] " << message << "\n";
    cout << "  [";
    for (int i = 0; i < size; ++i) {
        if (i > 0) cout << "   ";
        cout << "[";
        for (int j = 0; j < size; ++j) {
            cout << setw(3) << clock[i][j] << (j == size - 1 ? "" : ",");
        }
        cout << "]" << (i == size - 1 ? "" : ",\n");
    }
    cout << "]" << endl;
}

// update matrix clock fxn
void update_clock(vector<vector<int>>& local_clock, int rank, int size, const vector<vector<int>>* received_clock = nullptr) {
    local_clock[rank][rank]++;

    if (received_clock != nullptr) {
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                local_clock[i][j] = max(local_clock[i][j], (*received_clock)[i][j]);
            }
        }
    }
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (world_size < 2) {
        if (world_rank == 0) cerr << "This program requires at least 2 processes." << endl;
        MPI_Finalize();
        return 1;
    }

    vector<vector<int>> matrix_clock(world_size, vector<int>(world_size, 0));
    srand(time(NULL) + world_rank);

    MPI_Barrier(MPI_COMM_WORLD);
    if (world_rank == 0) cout << "--- Matrix Clock Simulation Starting ---" << endl;
    MPI_Barrier(MPI_COMM_WORLD);

    const int NUM_ITERATIONS = 10;
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        usleep(10000 * (1 + (rand() % 50))); 

        int flag = 0;
        MPI_Status status;
        MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);

        if (flag) {
            vector<vector<int>> recv_buffer(world_size, vector<int>(world_size, 0));
            int source_rank = status.MPI_SOURCE;
            MPI_Recv(recv_buffer.data()->data(), world_size * world_size, MPI_INT, source_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            cout << "[Rank " << world_rank << "] Received clock from Rank " << source_rank << "." << endl;
            update_clock(matrix_clock, world_rank, world_size, &recv_buffer);
            print_matrix_clock(matrix_clock, world_size, world_rank, "Updated after receive.");
        }

        int action_choice = rand() % 3;
        if (action_choice == 0) { // Send event
            update_clock(matrix_clock, world_rank, world_size);
            int dest_rank;
            do {
                dest_rank = rand() % world_size;
            } while (dest_rank == world_rank);

            MPI_Request send_request;
            MPI_Isend(matrix_clock.data()->data(), world_size * world_size, MPI_INT, dest_rank, 0, MPI_COMM_WORLD, &send_request); 
            print_matrix_clock(matrix_clock, world_size, world_rank, "Sent to Rank " + to_string(dest_rank) + ".");

        } 
        else { // Internal event
            update_clock(matrix_clock, world_rank, world_size);
            print_matrix_clock(matrix_clock, world_size, world_rank, "Internal event.");
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    if (world_rank == 0) cout << "\n--- Simulation Finished ---" << endl;
    MPI_Barrier(MPI_COMM_WORLD);

    for (int rank = 0; rank < world_size; ++rank) {
        if (world_rank == rank) {
            print_matrix_clock(matrix_clock, world_size, world_rank, "Final State.");
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}