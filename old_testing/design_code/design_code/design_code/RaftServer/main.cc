/**
 * Main function for starting up a Raft Server through initialization of the globals unit
 * 
 * Below is mockup/pseudocode of what needs to be passed in/started up
 * 
*/
#include "RaftGlobals.hh"

using namespace Raft;

int main() {
    // Step 1: Make whatever temp dirs we expect to be storing persistent state in, check for config(OR we do this in globals init)

    // Step 2: Create globals object
    Raft::Globals globals('./config_file_path');

    // Step 3: Use the Start() provided to us by globals
    globals.start();

    return 0;
}