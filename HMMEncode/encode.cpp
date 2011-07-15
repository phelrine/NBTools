#include "nbhmm.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <clx/table.h>
#include <clx/tokenizer.h>

#define DIRMODE (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

struct Config
{
  int seed;
  char dirname[BUFSIZ];
  double df; // IW's variance parameter
  double dS; // means variance parameter
  double kappa;
  double alpha;
  double gamma;
  int state;
  int iteration;
};

void ReadConfig(Config &config)
{
  std::cin >> config.seed;
  std::cin >> config.dirname;
  std::cin >> config.df;
  std::cin >> config.dS;
  std::cin >> config.kappa;
  std::cin >> config.alpha;
  std::cin >> config.gamma;
  std::cin >> config.state;
  std::cin >> config.iteration;
}

dgematrix ReadDataFile(const char *filename, bool hasHeader = true)
{
  std::ifstream in(filename);
  std::string buf;

  
  if(hasHeader){
    getline(in, buf);
  }


  clx::char_separator<char> sep(',');
  clx::table<std::string> table(in, clx::create_tokenizer<std::string>(sep));
  dgematrix matrix((int) table.size(), table[0].size());
  for (size_t i = 0; i < table.size(); i++){
    for(unsigned int j = 0; j < table[i].size(); j++){
      std::stringstream convertor(table[i][j]);
      convertor >> matrix(i, j);
    }
  }

  return matrix;
}

void WriteResults(const NBShdpHmm &hmm, const std::vector< std::vector<int> > &outputs)
{
  char filename[BUFSIZ];

  for(int i = 0; i < hmm.G.size(); i++){
    sprintf(filename, "%d_mu.dco", i);
    hmm.G[i].Mu.write(filename);
    sprintf(filename, "%d_sig.dge", i);
    hmm.G[i].Sig.write(filename);
  }

  for(int i = 0; i < outputs.size(); i++){
    sprintf(filename, "%d_output.dco", i);
    dco(outputs[i]).write(filename);
  }
}

void MakeAndChangeDirectory(const char *dirname)
{
  if(mkdir(dirname, DIRMODE) != 0){
    std::cerr << "can't make directory " << dirname << std::endl;
    exit(1);
  }
  if(chdir(dirname) != 0){
    std::cerr << "can't change directory " << dirname << std::endl;
    exit(1);
  }
}

int main(int argc, char *argv[])
{
  Config config;
  ReadConfig(config);
  
  srand(config.seed);
  srandom(config.seed);

  std::vector<dgematrix> observations;
  int input_count(argc - 1), min_dim(100);
  
  for(int i = 1; i <= input_count; i++){
    dgematrix data = ReadDataFile(argv[i]);
    observations.push_back(data);
    min_dim = std::min(min_dim, (int)data.n);
  }

  NBShdpHmm estimate;
  estimate.resize(config.state, min_dim);
  for(int i = 0; i < config.state; i++){
    estimate.G[i].hp_A.identity();
    estimate.G[i].hp_A = config.df * estimate.G[i].hp_A;
    estimate.G[i].hp_S.identity();
    estimate.G[i].hp_S = config.dS * estimate.G[i].hp_S;
    estimate.hp_kappa = config.kappa;
    estimate.hp_alpha = config.alpha;
    estimate.hp_gamma = config.gamma;
    estimate.G[i].Mu = MultiGaussSampler(estimate.G[i].hp_m,estimate.G[i].hp_S);
  }

  std::vector< std::vector<int> > outputs;
  outputs.resize(input_count);

  MakeAndChangeDirectory(config.dirname);
  for(int i = 0; i < config.iteration; i++){
    // dcovector likely_log(config.iteration), num_log(config.iteration);    
    for(int j = 0; j < input_count; j++){
      dgematrix backward;
      backward = BackwardFiltering(estimate, observations[j]);
      outputs[j] = ForwardSampling(estimate, backward, observations[j]);
    }
    estimate.Update_shdp_multi(observations, outputs);

    char dirname[BUFSIZ];
    sprintf(dirname, "%d_itr", i);
    MakeAndChangeDirectory(dirname);
    WriteResults(estimate, outputs);
    chdir("..");
  }

  return 0;
}