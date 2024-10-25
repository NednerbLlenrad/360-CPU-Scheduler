#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Headers as needed

typedef enum {false, true} bool;        // Allows boolean types in C

/* Defines a job struct */
typedef struct Process {
    uint32_t A;                         // A: Arrival time of the process
    uint32_t B;                         // B: Upper Bound of CPU burst times of the given random integer list
    uint32_t C;                         // C: Total CPU time required
    uint32_t M;                         // M: Multiplier of CPU burst time
    uint32_t processID;                 // The process ID given upon input read

    uint8_t status;                     // 0 is unstarted, 1 is ready, 2 is running, 3 is blocked, 4 is terminated

    int32_t finishingTime;              // The cycle when the the process finishes (initially -1)
    uint32_t currentCPUTimeRun;         // The amount of time the process has already run (time in running state)
    uint32_t currentIOBlockedTime;      // The amount of time the process has been IO blocked (time in blocked state)
    uint32_t currentWaitingTime;        // The amount of time spent waiting to be run (time in ready state)

    uint32_t IOBurst;                   // The amount of time until the process finishes being blocked
    uint32_t CPUBurst;                  // The CPU availability of the process (has to be > 1 to move to running)

    int32_t quantum;                    // Used for schedulers that utilise pre-emption

    bool isFirstTimeRunning;            // Used to check when to calculate the CPU burst when it hits running mode

    struct Process* nextInBlockedList;  // A pointer to the next process available in the blocked list
    struct Process* nextInReadyQueue;   // A pointer to the next process available in the ready queue
    struct Process* nextInReadySuspendedQueue; // A pointer to the next process available in the ready suspended queue
} _process;


uint32_t CURRENT_CYCLE = 0;             // The current cycle that each process is on
uint32_t TOTAL_CREATED_PROCESSES = 0;   // The total number of processes constructed
uint32_t TOTAL_STARTED_PROCESSES = 0;   // The total number of processes that have started being simulated
uint32_t TOTAL_FINISHED_PROCESSES = 0;  // The total number of processes that have finished running
uint32_t TOTAL_NUMBER_OF_CYCLES_SPENT_BLOCKED = 0; // The total cycles in the blocked state

const char* RANDOM_NUMBER_FILE_NAME= "random-numbers";
const uint32_t SEED_VALUE = 200;  // Seed value for reading from file

// Additional variables as needed


/**
 * Reads a random non-negative integer X from a file with a given line named random-numbers (in the current directory)
 */
uint32_t getRandNumFromFile(uint32_t line, FILE* random_num_file_ptr){
    uint32_t end, loop;
    char str[512];

    rewind(random_num_file_ptr); // reset to be beginning
    for(end = loop = 0;loop<line;++loop){
        if(0==fgets(str, sizeof(str), random_num_file_ptr)){ //include '\n'
            end = 1;  //can't input (EOF)
            break;
        }
    }
    if(!end) {
        return (uint32_t) atoi(str);
    }

    // fail-safe return
    return (uint32_t) 1804289383;
}

/*Parsing*/
void parseFile(const char *filename, _process process_list[], uint32_t *num_processes) {
    FILE* input = fopen(filename, "r");
    if (!input) {
        printf("Error: File unable to be opened!\n");
        exit(1);
    }

    // Scan in a single line of the file
    char line[256];
    if (fgets(line, sizeof(line), input)) {
        // Read the first token as the number of processes
        sscanf(line, "%u", num_processes);
        //printf("The original input was: %u\n", *num_processes);  
        TOTAL_CREATED_PROCESSES = *num_processes;
        uint32_t process_index = 0;
        uint32_t A, B, C, M;

        // Start tokenizing
        char *ptr = line;
        while (process_index < *num_processes) {

            ptr = strchr(ptr, '(');
            if (ptr == NULL) {
                break;  
            }

            // Parse the values in the parentheses
            if (sscanf(ptr, "(%u %u %u %u)", &A, &B, &C, &M) == 4) {
                printf("Parsed process: A=%u, B=%u, C=%u, M=%u\n", A, B, C, M);
                
                _process new_process = {
                    .A = A,
                    .B = B,
                    .C = C,
                    .M = M,
                    .processID = process_index,
                    .status = 0,
                    .finishingTime = -1,
                    .currentCPUTimeRun = 0,
                    .currentIOBlockedTime = 0,
                    .currentWaitingTime = 0,
                    .IOBurst = 0,
                    .CPUBurst = 0,
                    .quantum = 0,
                    .isFirstTimeRunning = true,
                    .nextInBlockedList = NULL,
                    .nextInReadyQueue = NULL,
                    .nextInReadySuspendedQueue = NULL
                };
                
                process_list[process_index++] = new_process;
            }
            ptr++; 
        }

        //Not enough processes
        if (process_index < *num_processes) {
            printf("Warning: Expected %u processes, but found only %u.\n", *num_processes, process_index);
            *num_processes = process_index;  
        }
    } else {
        printf("Error: Failed to read from the file.\n");
    }

    fclose(input);
}





/**
 * Reads a random non-negative integer X from a file named random-numbers.
 * Returns the CPU Burst: : 1 + (random-number-from-file % upper_bound)
 */
uint32_t randomOS(uint32_t upper_bound, uint32_t process_indx, FILE* random_num_file_ptr)
{
    char str[20];
    
    uint32_t unsigned_rand_int = (uint32_t) getRandNumFromFile(SEED_VALUE+process_indx, random_num_file_ptr);
    uint32_t returnValue = 1 + (unsigned_rand_int % upper_bound);

    return returnValue;
} 


/********************* SOME PRINTING HELPERS *********************/


/**
 * Prints to standard output the original input
 * process_list is the original processes inputted (in array form)
 */
void printStart(_process process_list[])
{
    printf("The original input was: %i", TOTAL_CREATED_PROCESSES);

    uint32_t i = 0;
    for (; i < TOTAL_CREATED_PROCESSES; ++i)
    {
        printf(" ( %i %i %i %i)", process_list[i].A, process_list[i].B,
               process_list[i].C, process_list[i].M);
    }
    printf("\n");
} 

/**
 * Prints to standard output the final output
 * finished_process_list is the terminated processes (in array form) in the order they each finished in.
 */
void printFinal(_process finished_process_list[])
{
    printf("The (sorted) input is: %i", TOTAL_CREATED_PROCESSES);

    uint32_t i = 0;
    for (; i < TOTAL_FINISHED_PROCESSES; ++i)
    {
        printf(" ( %i %i %i %i)", finished_process_list[i].A, finished_process_list[i].B,
               finished_process_list[i].C, finished_process_list[i].M);
    }
    printf("\n");
} // End of the print final function

/**
 * Prints out specifics for each process.
 * @param process_list The original processes inputted, in array form
 */
void printProcessSpecifics(_process process_list[])
{
    uint32_t i = 0;
    printf("\n");
    for (; i < TOTAL_CREATED_PROCESSES; ++i)
    {
        printf("Process %i:\n", process_list[i].processID);
        printf("\t(A,B,C,M) = (%i,%i,%i,%i)\n", process_list[i].A, process_list[i].B,
               process_list[i].C, process_list[i].M);
        printf("\tFinishing time: %i\n", process_list[i].finishingTime);
        printf("\tTurnaround time: %i\n", process_list[i].finishingTime - process_list[i].A);
        printf("\tI/O time: %i\n", process_list[i].currentIOBlockedTime);
        printf("\tWaiting time: %i\n", process_list[i].currentWaitingTime);
        printf("\n");
    }
} // End of the print process specifics function

/**
 * Prints out the summary data
 * process_list The original processes inputted, in array form
 */
void printSummaryData(_process process_list[])
{
    uint32_t i = 0;
    double total_amount_of_time_utilizing_cpu = 0.0;
    double total_amount_of_time_io_blocked = 0.0;
    double total_amount_of_time_spent_waiting = 0.0;
    double total_turnaround_time = 0.0;
    uint32_t final_finishing_time = CURRENT_CYCLE - 1;
    for (; i < TOTAL_CREATED_PROCESSES; ++i)
    {
        total_amount_of_time_utilizing_cpu += process_list[i].currentCPUTimeRun;
        total_amount_of_time_io_blocked += process_list[i].currentIOBlockedTime;
        total_amount_of_time_spent_waiting += process_list[i].currentWaitingTime;
        total_turnaround_time += (process_list[i].finishingTime - process_list[i].A);
    }

    // Calculates the CPU utilisation
    double cpu_util = total_amount_of_time_utilizing_cpu / final_finishing_time;

    // Calculates the IO utilisation
    double io_util = (double) TOTAL_NUMBER_OF_CYCLES_SPENT_BLOCKED / final_finishing_time;

    // Calculates the throughput (Number of processes over the final finishing time times 100)
    double throughput =  100 * ((double) TOTAL_CREATED_PROCESSES/ final_finishing_time);

    // Calculates the average turnaround time
    double avg_turnaround_time = total_turnaround_time / TOTAL_CREATED_PROCESSES;

    // Calculates the average waiting time
    double avg_waiting_time = total_amount_of_time_spent_waiting / TOTAL_CREATED_PROCESSES;

    printf("Summary Data:\n");
    printf("\tFinishing time: %i\n", CURRENT_CYCLE - 1);
    printf("\tCPU Utilisation: %6f\n", cpu_util);
    printf("\tI/O Utilisation: %6f\n", io_util);
    printf("\tThroughput: %6f processes per hundred cycles\n", throughput);
    printf("\tAverage turnaround time: %6f\n", avg_turnaround_time);
    printf("\tAverage waiting time: %6f\n", avg_waiting_time);
} // End of the print summary data function

/*Simulation Functions*/
/*First Come First Serve*/
void simFCFS(_process process_list[], uint32_t num_processes, FILE* random_num_file_ptr) {
    uint32_t time = 0;  // Local time variable for the simulation
    uint32_t finished_count = 0;
    uint32_t total_io_time = 0; // To track total I/O time for all processes
    uint32_t total_cpu_time = 0; // To track total CPU time for utilization calculations

    printf("Running FCFS Scheduler with %u processes\n", num_processes);

    // Loop until all processes are finished
    while (finished_count < num_processes) {
        for (uint32_t i = 0; i < num_processes; i++) {
            _process *proc = &process_list[i];

            // Set process to ready if it has arrived
            if (proc->status == 0 && proc->A <= time) {
                proc->status = 1; // Ready state
            }

            // If the process is in the ready state, move it to running
            if (proc->status == 1) {
                proc->status = 2; // Running state
                proc->CPUBurst = randomOS(proc->B, proc->processID, random_num_file_ptr); // Get CPU burst time

                // Process execution simulation
                while (proc->CPUBurst > 0 && proc->currentCPUTimeRun < proc->C) {
                    proc->CPUBurst--;
                    proc->currentCPUTimeRun++;
                    time++;
                    CURRENT_CYCLE++; // Increment the global CURRENT_CYCLE during execution
                    total_cpu_time++; // Increment total CPU time for utilization calculation
                }

                // Check if process finished total CPU time
                if (proc->currentCPUTimeRun == proc->C) {
                    proc->status = 4; // Finished state
                    proc->finishingTime = time; // Set finishing time
                    proc->currentWaitingTime = (proc->finishingTime - proc->A - proc->currentCPUTimeRun); // Calculate waiting time
                    finished_count++; // Move to next process
                } else {
                    // If process still has remaining CPU time, move it to blocked state for I/O
                    proc->status = 3; // Blocked state (I/O)
                    proc->IOBurst = randomOS(proc->M, proc->processID, random_num_file_ptr); // Get I/O burst time
                }
            }

            // If process is in blocked state (I/O), simulate I/O burst
            if (proc->status == 3) {
                if (proc->IOBurst > 0) {
                    proc->IOBurst--;
                    proc->currentIOBlockedTime++;  // Track total I/O time for the current process
                    total_io_time++; // Increment total I/O time across all processes
                    TOTAL_NUMBER_OF_CYCLES_SPENT_BLOCKED++; // Increment the total blocked cycles
                }

                if (proc->IOBurst == 0) {
                    proc->status = 1; // Move back to ready state after I/O
                }
            }
        }

        // Increment time for the next cycle if no process is running
        if (finished_count < num_processes) {
            time++;
        }
    }

}
/*Round Robin*/
void simRR(_process process_list[], uint32_t num_processes, uint32_t quantum, FILE* random_num_file_ptr) {
    uint32_t time = 0;
    uint32_t finished_count = 0;
    
    //printf("Entering RR...\n");
    
    // Initialize quantum for each process
    for (uint32_t i = 0; i < num_processes; i++) {
        process_list[i].quantum = quantum;
    }

    // Loop through the processes in round robin until all are finished
    while (finished_count < num_processes) {
        bool all_blocked = true;  // Track if all processes are blocked

        for (uint32_t i = 0; i < num_processes; i++) {
            _process *proc = &process_list[i];
            //printf("Time: %d, Simulating process %d, Status: %d, Quantum: %d, CPUBurst: %d\n", time, i, proc->status, proc->quantum, proc->CPUBurst);

            // Process arrives and moves to ready state
            if (proc->status == 0 && proc->A <= time) {
                proc->status = 1; // Ready state
                //printf("Process %d ready.\n", i);
            }

            // If the process is in ready state, move it to running
            if (proc->status == 1) {
                all_blocked = false;  // There's at least one process ready
                if (proc->CPUBurst == 0) {  // If no burst time is set
                    proc->CPUBurst = randomOS(proc->B, proc->processID, random_num_file_ptr);
                    //printf("Process %d new CPU burst: %d\n", i, proc->CPUBurst);
                }
                proc->status = 2; // Running state
                //printf("Process %d running.\n", i);                
                // Execute for one unit of time
                proc->CPUBurst--;
                proc->currentCPUTimeRun++;
                proc->quantum--;

                time++;
                CURRENT_CYCLE++;

                // Process completed CPU burst
                if (proc->CPUBurst == 0) {
                    proc->status = 3; // Blocked state
                    proc->IOBurst = randomOS(proc->M, proc->processID, random_num_file_ptr); // Simulate I/O burst based on multiplier
                    //printf("Process %d blocked with IO burst: %d.\n", i, proc->IOBurst);  
                }

                // Process finished total execution
                if (proc->currentCPUTimeRun == proc->C) {
                    proc->status = 4; // Finished
                    proc->finishingTime = time;
                    proc->currentWaitingTime = time - proc->A - proc->currentCPUTimeRun;
                    finished_count++;
                    //printf("Process %d finished.\n", i);  
                }

                // Quantum exhausted but process not finished, move back to ready
                if (proc->quantum == 0 && proc->status != 4) {
                    proc->status = 1; // Back to ready state
                    proc->quantum = quantum; // Reset quantum for next cycle
                    //printf("Process %d quantum expired, moving back to ready.\n", i);
                }
            }

            // Handle blocked processes (simulate I/O burst)
            if (proc->status == 3) {
                proc->IOBurst--;  // Simulate I/O burst time
                //printf("Process %d in IO, IO burst remaining: %d\n", i, proc->IOBurst);
                if (proc->IOBurst == 0) {
                    proc->status = 1;  // Move back to ready after I/O
                    proc->quantum = quantum; // Reset quantum after I/O completion
                    //printf("Process %d IO complete, moving to ready.\n", i);
                }
            }
        }

        // If all processes are blocked, time must still advance
        if (finished_count < num_processes && all_blocked) {
            //printf("All processes blocked, advancing time.\n");
            time++;
        }
    }
}


/*Shortest Job First*/
void simSJF(_process process_list[], uint32_t num_processes, FILE *random_num_file_ptr) {
    uint32_t time = 0;
    uint32_t finished_count = 0;
    uint32_t total_io_time = 0; // Total I/O time for utilization
    uint32_t total_cpu_time = 0; // Total CPU time for utilization

    printf("Running SJF Scheduler with %u processes\n", num_processes);

    while (finished_count < num_processes) {
        _process *shortest = NULL;

        // Check for available processes to run
        for (uint32_t i = 0; i < num_processes; ++i) {
            _process *proc = &process_list[i];

            // Set process to ready if it has arrived
            if (proc->status == 0 && proc->A <= time) {
                proc->status = 1; // Ready state
            }

            // Finding the shortest job among ready processes
            if (proc->status == 1 || (proc->status == 0 && proc->A <= time)) {
                if (!shortest || (proc->C - proc->currentCPUTimeRun) < (shortest->C - shortest->currentCPUTimeRun)) {
                    shortest = proc;
                }
            }
        }

        if (shortest) {
            // Move to running state
            shortest->status = 2;
            shortest->CPUBurst = randomOS(shortest->B, shortest->processID, random_num_file_ptr); // Get CPU burst time

            // Process execution simulation
            while (shortest->CPUBurst > 0 && shortest->currentCPUTimeRun < shortest->C) {
                shortest->CPUBurst--;
                shortest->currentCPUTimeRun++;
                time++;
                total_cpu_time++; // Increment total CPU time for utilization calculation
            }

            // Check if process finished total CPU time
            if (shortest->currentCPUTimeRun == shortest->C) {
                shortest->status = 4; // Finished state
                shortest->finishingTime = time; // Set finishing time
                shortest->currentWaitingTime = (shortest->finishingTime - shortest->A - shortest->currentCPUTimeRun); // Calculate waiting time
                finished_count++; // Move to the next process
            } else {
                // If process still has remaining CPU time, move it to blocked state for I/O
                shortest->status = 3; // Blocked state (I/O)
                shortest->IOBurst = randomOS(shortest->M, shortest->processID, random_num_file_ptr); // Get I/O burst time
            }
        }

        // Increment time for the next cycle if no process is running
        if (finished_count < num_processes) {
            time++;
        }

        // Track I/O time for any blocked processes
        for (uint32_t i = 0; i < num_processes; ++i) {
            _process *proc = &process_list[i];

            if (proc->status == 3) { // If in blocked state (I/O)
                if (proc->IOBurst > 0) {
                    proc->IOBurst--;
                    proc->currentIOBlockedTime++;  // Track total I/O time for the current process
                    total_io_time++; // Increment total I/O time across all processes
                }

                if (proc->IOBurst == 0) {
                    proc->status = 1; // Move back to ready state after I/O
                }
            }
        }
    }
}

/*Reset process state*/
void resetProcessStates(_process process_list[], uint32_t num_processes) {
    for (uint32_t i = 0; i < num_processes; ++i) {
        process_list[i].status = 0; // Reset status to unstarted
        process_list[i].finishingTime = -1; // Reset finishing time
        process_list[i].currentCPUTimeRun = 0; // Reset CPU time run
        process_list[i].currentIOBlockedTime = 0; // Reset I/O blocked time
        process_list[i].currentWaitingTime = 0; // Reset waiting time
        process_list[i].IOBurst = 0; // Reset I/O burst
        process_list[i].CPUBurst = 0; // Reset CPU burst
        process_list[i].quantum = 0; // Reset quantum
        process_list[i].isFirstTimeRunning = true; // Reset running flag
    }
}

/**
 * The magic starts from here
 */
int main(int argc, char *argv[])
{
        // Other variables
    if (argc < 2) {
        printf("Usage: %s <input_file>\n",argv[1]);
        return 1;
    }

    const char *input_file = argv[1];
    uint32_t total_num_of_process = 0;               // Read from the file -- number of process to create
    _process process_list[100]; // Creates a container for all processes
    if (!process_list){
        printf("Error: Process list not formed.");
        return 1;
    }

    FILE* random_num_file_ptr = fopen(RANDOM_NUMBER_FILE_NAME, "r");
    if (!random_num_file_ptr){
        printf("Error: Could not open %s.\n", RANDOM_NUMBER_FILE_NAME);
        return 1;
    } 
    // Write code for your shiny scheduler
    parseFile(input_file, process_list, &total_num_of_process);


    //First Come First Serve
    printf("Running FCFS...\n");
    printStart(process_list);
    printFinal(process_list);
    simFCFS(process_list, total_num_of_process, random_num_file_ptr);
    printProcessSpecifics(process_list);
    printSummaryData(process_list);
    resetProcessStates(process_list, total_num_of_process);
    
    //Round Robin
    printf("Running RR...\n");
    printStart(process_list);
    printFinal(process_list);
    simRR(process_list, total_num_of_process, 2, random_num_file_ptr);
    printProcessSpecifics(process_list);
    printSummaryData(process_list);
    resetProcessStates(process_list, total_num_of_process);

    //Shortest Job First    
    
    printf("Running SJF...\n");
    printStart(process_list);
    printFinal(process_list);
    simSJF(process_list, total_num_of_process, random_num_file_ptr);
    printProcessSpecifics(process_list);
    printSummaryData(process_list);
    
    return 0;
} 