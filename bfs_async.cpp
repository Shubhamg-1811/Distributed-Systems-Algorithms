#include <iostream>
#include <vector>
#include <algorithm>
#include <mpi.h>
#include <unistd.h> 
using namespace std;

const int MC_PROPOSE_TAG = 10;
const int MP_ACCEPT_TAG = 11;
const int MR_REJECT_TAG = 12;
const int MS_SYNC_TAG    = 13; 
const int MC_COMPLETE_TAG = 14; 
const int M_TERMINATE_TAG = 15;
const int ROOT_RANK = 0; 

struct RSTMessage {
    int sender_rank;
};
vector<vector<int>> get_graph_topology(int world_size) {
    if (world_size == 4) {
        return {{1, 3},{0, 2},{1, 3},{0, 2}};
    } 
    return vector<vector<int>>(world_size); 
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (world_size < 2) {
        if (world_rank == 0) cerr << "at least 2 processes required" << endl;
        MPI_Finalize();
        return 0;
    }

    const vector<vector<int>> adjacency_list = get_graph_topology(world_size);
    if (adjacency_list.empty()) {
         if (world_rank == 0) cerr << "Error: No topology defined for size " << world_size << endl;
         MPI_Finalize();
         return 1;
    }
    const vector<int>& neighbors = adjacency_list[world_rank];
    const int num_neighbors = neighbors.size();

    int parent_rank = (world_rank == ROOT_RANK) ? -1 : -2; 
    vector<int> children;
    int level_status = 0; 
    int no_response_remaining = 0; 
    int children_yet_to_complete = 0;

    RSTMessage received_msg;
    MPI_Request recv_request; 
    MPI_Status recv_status;
    
    if (world_rank == ROOT_RANK) {
        cout << "\nRank " << world_rank << " (ROOT) initiating Level 0 proposals." << endl;
        RSTMessage send_mc_msg = {world_rank};
        for (int dest_rank : neighbors) {
            MPI_Send(&send_mc_msg, sizeof(RSTMessage), MPI_BYTE, dest_rank, MC_PROPOSE_TAG, MPI_COMM_WORLD);
        }
        no_response_remaining = num_neighbors;
        level_status = 1;
    } 
    else {
        cout << "Rank " << world_rank << ": Waiting for first MC message to select parent." << endl;
        MPI_Recv(&received_msg, sizeof(RSTMessage), MPI_BYTE, MPI_ANY_SOURCE, MC_PROPOSE_TAG, MPI_COMM_WORLD, &recv_status);
        
        parent_rank = received_msg.sender_rank;
        cout << "Rank " << world_rank << ": First MC received from " << parent_rank << ". Parent set to " << parent_rank << "." << endl;
        
        RSTMessage send_mp_msg = {world_rank};
        MPI_Send(&send_mp_msg, sizeof(RSTMessage), MPI_BYTE, parent_rank, MP_ACCEPT_TAG, MPI_COMM_WORLD);        
        level_status = 0; 
    }
    MPI_Irecv(&received_msg, sizeof(RSTMessage), MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &recv_request);
    bool loop_active = true;
    while (loop_active) {
        usleep(10000); 
        int flag = 0;
        MPI_Test(&recv_request, &flag, &recv_status);

        if (flag) { 
            int sender_rank = received_msg.sender_rank;
            int received_tag = recv_status.MPI_TAG;
            MPI_Irecv(&received_msg, sizeof(RSTMessage), MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &recv_request);
            if (received_tag == MP_ACCEPT_TAG) {
                if (level_status == 1) {
                    children.push_back(sender_rank);
                    no_response_remaining--;
                    cout << "Rank " << world_rank << ": Accepted as parent by " << sender_rank << " (MP). Resp left: " << no_response_remaining << endl;
                }
            } 
            else if (received_tag == MC_COMPLETE_TAG) {
                if (level_status == 2) {
                    children_yet_to_complete--;
                    cout << "Rank " << world_rank << ": Child " << sender_rank << " reported completion. " << children_yet_to_complete << " children left." << endl;
                }
            }
            else if (received_tag == MR_REJECT_TAG) {
                if (level_status == 1) {
                    no_response_remaining--;
                    cout << "Rank " << world_rank << ": Rejected by " << sender_rank << " (MR). Resp left: " << no_response_remaining << endl;
                }
            }
            else if (received_tag == MS_SYNC_TAG) {
                if (world_rank != ROOT_RANK && sender_rank == parent_rank && level_status == 0) {
                    level_status = 3;
                    cout << "Rank " << world_rank << ": Received MS from parent " << parent_rank << ". STARTING PROPOSALS." << endl;
                }
            }
            else if (received_tag == MC_PROPOSE_TAG) {
                RSTMessage send_mr_msg = {world_rank};
                if (parent_rank != -2) { 
                    MPI_Send(&send_mr_msg, sizeof(RSTMessage), MPI_BYTE, sender_rank, MR_REJECT_TAG, MPI_COMM_WORLD);
                    cout << "Rank " << world_rank << ": Rejected late MC proposal from " << sender_rank << " (sent MR)." << endl;
                } 
            }
            else if (received_tag == M_TERMINATE_TAG) {
                if (world_rank != ROOT_RANK) {
                    cout << "Rank " << world_rank << ": Received TERMINATE from ROOT. Shutting down." << endl;
                    loop_active = false;
                }
            }
        }
        if (level_status == 3) {
            RSTMessage send_mc_msg = {world_rank};
            int proposals_sent = 0;
            for (int dest_rank : neighbors) {
                if (dest_rank != parent_rank) {
                    MPI_Send(&send_mc_msg, sizeof(RSTMessage), MPI_BYTE, dest_rank, MC_PROPOSE_TAG, MPI_COMM_WORLD);
                    cout << "Rank " << world_rank << ": Sent MC to neighbor " << dest_rank << endl;
                    proposals_sent++;
                }
            }
            no_response_remaining = proposals_sent; 
            level_status = 1;             
            
            if (proposals_sent == 0) {
                cout << "Rank " << world_rank << ": Is a LEAF node. No proposals to send." << endl;
                level_status = 2;
                children_yet_to_complete = 0;
            }
        }
        if (level_status == 1 && no_response_remaining == 0) {
            level_status = 2;
            children_yet_to_complete = children.size();
            cout << "Rank " << world_rank << ": Finished proposals. Sending MS_SYNC to " << children_yet_to_complete << " children." << endl;
            RSTMessage send_ms_msg = {world_rank};
            for(int child_rank : children) {
                MPI_Send(&send_ms_msg, sizeof(RSTMessage), MPI_BYTE, child_rank, MS_SYNC_TAG, MPI_COMM_WORLD);
                cout << "Rank " << world_rank << ": Sent MS to child " << child_rank << " to start its proposals." << endl;
            }
            if (children_yet_to_complete == 0) {
                level_status = 4; 
            }
        }
        if (level_status == 2 && children_yet_to_complete == 0) {
             if (world_rank == ROOT_RANK) {
                cout << "Rank " << world_rank << " (ROOT): All children reported completion. Broadcasting TERMINATE." << endl;
                level_status = 5; 
                loop_active = false;

                RSTMessage terminate_msg = {world_rank};
                for (int i = 1; i < world_size; ++i) {
                    MPI_Send(&terminate_msg, sizeof(RSTMessage), MPI_BYTE, i, M_TERMINATE_TAG, MPI_COMM_WORLD);
                }
             } 
             else level_status = 4; 
        }
        if (level_status == 4) {
            RSTMessage send_complete_msg = {world_rank};
            MPI_Send(&send_complete_msg, sizeof(RSTMessage), MPI_BYTE, parent_rank, MC_COMPLETE_TAG, MPI_COMM_WORLD);
            cout << "Rank " << world_rank << ": Subtree complete. Sent MC_COMPLETE to parent " << parent_rank << endl;
            level_status = 5; 
            cout << "Rank " << world_rank << ": Moving to state 5 (Finished). Waiting for TERMINATE." << endl;
        }
    }    
    MPI_Cancel(&recv_request);
    MPI_Status status;
    MPI_Wait(&recv_request, &status);    
    MPI_Barrier(MPI_COMM_WORLD); 

    cout << "\n--- Rank " << world_rank << " BFS Result ---" << endl;
    cout << "Parent: " << ((world_rank == ROOT_RANK) ? "ROOT" : to_string(parent_rank)) << endl;
    cout << "Children (" << children.size() << "): ";
    if (children.empty()) cout << "None" << endl;
    else {
        for (int c : children) cout << c << " ";
        cout << endl;
    }
    cout << "--------------------------------" << endl;
    MPI_Finalize();
    return 0;
}