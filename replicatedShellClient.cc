/**
 * At this layer of abstraction, we know:
 *  - the "state machine" in question is a replicated shell
 *  - a list of servers will be passed in through a configuration file TODO: finalize config file
 *  - we are using socket programming to enable communication
 * To prevent information leakage, these are all the project 1 specific aspects/implementation
 * that should be separable from a Raft implementation that could hypothetically be reused and
 * socket programming code that code be reused.
 * 
 * The replicatedShellClient application will be responsible for running a simple loop:
 *  (a) read a one-line shell command from standard input
 *  (b) send the command to the cluster leader
 *  (c) print on standard output the results returned by the leader
 * 
 * Below is a mockup/pseudocode of the replicatedShellClient
 * This application will be called from the command line:
 *  user raft1 % ./replicatedShellClient
 * 
*/

int main() {
    // Step 1: parse configuration file to know what servers I can communicate with
    ServerConfig config = read(./config_file_path)

    // Step 2: Initialize RaftClient Object with list of servers
    RaftClient myRaftConsesusUnit = RaftClient(config)

    // Step 3: Launch application 
    char inbuf[256];
    while (1) {
        memset(inbuf, 0, 256);

        // (a) read line from stdin
        std::getline(std::in, inbuf);

        // (b) send the command to the cluster leader
        myRaftConsesusUnit.sendCommand(inbuf);

        // (b.2) read a response 
        std::string ret = myRaftConsesusUnit.readResponse();

        // (c) print return value on stdout
        cout << ret << endl;
    }

    return 0;
}