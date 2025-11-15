#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <mpi.h>
using namespace std;
int w_s;

// print vector clock fxn
void print_vc(int rank, const string& message, const vector<int>& vc) {
    stringstream ss;
    ss << "[Process " << rank << "] " << message << " VC: [";
    for (size_t i = 0; i < vc.size(); ++i) {
        ss << vc[i] << (i == vc.size() - 1 ? "" : ", ");
    }
    ss << "]";
    cout << ss.str() << endl;
}

// update vector clock fxn
void update(vector<int>& my_vc, int world_rank, const vector<int>& recv_vc) {
    if (recv_vc.empty()) {
        my_vc[world_rank]++;
        return;
    }
    for (int k = 0; k < w_s; ++k) {
        my_vc[k] = max(my_vc[k], recv_vc[k]);
    }
    my_vc[world_rank]++;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    w_s = world_size;
    srand(time(NULL) + world_rank);

    vector<int> my_vc(world_size, 0);
    const int NUM_ACTIONS = 10; 

    for (int i = 0; i < NUM_ACTIONS; ++i) {
        usleep((rand() % 80 + 20) * 1000);
        int flag = 0;
        MPI_Status status;

        MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
            int source = status.MPI_SOURCE;
            vector<int> received_vc(world_size);
            MPI_Recv(received_vc.data(), world_size, MPI_INT, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            cout << "[Process " << world_rank << "] Received clock from Process " << source << "." << endl;
            update(my_vc, world_rank, received_vc);
            print_vc(world_rank, "Updated after receive.", my_vc);
        }

        int action_choice = rand() % 3;

        if (action_choice == 0) { // Send event
            update(my_vc, world_rank, {});
            int dest = rand() % world_size;
            while (dest == world_rank) {
                dest = rand() % world_size;
            }
            MPI_Request send_request;
            MPI_Isend(my_vc.data(), world_size, MPI_INT, dest, 0, MPI_COMM_WORLD, &send_request);
            cout << "[Process " << world_rank << "] Sent clock to Process " << dest << "." << endl;
        } 
        else { // Internal event
            update(my_vc, world_rank, {});
            print_vc(world_rank, "Internal event.", my_vc);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    usleep(500 * 1000);

    if (world_rank == 0) {
        cout << "\n--- FINAL STATES ---\n" << endl;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    print_vc(world_rank, "Final state", my_vc);

    MPI_Finalize();
    return 0;
}