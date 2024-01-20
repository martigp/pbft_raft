/**
 * At this layer of abstraction, we know:
 *  - the "state machine" in question is a replicated shell
 *  - a list of servers will be passed in through a configuration file TODO: finalize config file
 *  - persistent state will be stored in TODO: decide names of files to store(hidden directory?)
 *  - we are using socket programming to enable communication
 * To prevent information leakage, these are all the project 1 specific aspects/implementation
 * that should be separable from a Raft implementation that could hypothetically be reused and
 * socket programming code that code be reused.
 * 
 * Below is a mockup/pseudocode of the replicatedShellServer
 * This application will be called from the command line:
 *  user raft1 % ./replicatedShellServer
 * 
*/

int main() {
    // Step 1: parse configuration file and decide what server I am
    ServerConfig config = read(./config_file_path)

    // Step 2: Initialize RaftServer Object with a ReplicatedShell State Machine
    RaftServer myRaftConsesusUnit = RaftServer(config, ReplicatedShell)

    // Step 3: Initialize timer thread
    std::thread timerThread = std::thread(startTimer, 0);

    // Step 4: Initialize epoll/kqueue listening thread with listening ports for all servers
    std::thread socketEventThread = std::thread(startSocketEventThread, 1);

    // Step 5: join?
    timerThread.join();
    socketEventThread.join();

    return 0;
}