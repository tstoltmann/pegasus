#ifndef MASTER_H
#define MASTER_H

#include <queue>

#include "engine.h"
#include "dag.h"

class Master {
    std::string program;
    std::string dagfile;
    std::string outfile;
    std::string errfile;
    DAG *dag;
    Engine *engine;
    std::queue<int> idle;
    
    int numworkers;

    long total_count;
    long success_count;
    long failed_count;
    
    void hostrank_workers();
    
    void submit_task(Task *t, int worker);
    void wait_for_result();
    void add_worker(int worker);
    bool has_idle_worker();
    void mark_worker_idle(int worker);
    int next_idle_worker();
    void merge_task_stdio(FILE *dest, const std::string &src, const std::string &stream);
public:
    Master(const std::string &program, Engine &engine, DAG &dag, const std::string &dagfile, const std::string &outfile, const std::string &errfile);
    ~Master();
    int run();
};

#endif /* MASTER_H */
