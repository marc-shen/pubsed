#ifndef _NLTE_ATOM_H
#define _NLTE_ATOM_H 1

#include <vector>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_multiroots.h>
#include "xy_array.h"
#include "locate_array.h"

struct fuzz_line
{
  double nu;
  double El;
  double gf;
  int ion;
  int bin;
};


struct nlte_ion
{
  int stage;          // ionization stage (0 = neutral, 1 = +, etc..)
  int ground;         // index of ground state level
  double chi;         // ionization energy, in eV
  double part;        // partition function
  double frac;        // fractional abundance
};

struct nlte_line
{
  int lu,ll;           // index of upper/lower level
  double lam;          // rest wavelength (angstroms)
  double f_lu;         // oscillator strength
  double A_ul;         // Einstein A coeficient
  double B_ul;         // Einstein B coeficient
  double B_lu;         // Einstein B coeficient
  double J;            // radiation field in line
  double tau;          // sobolev optical depth
  double etau;         // exponential of soblev optical depth
  double beta;         // sobolev escape probability
  int    bin;          // index of the nu grid bin
};

struct nlte_level
{
  int   id;             // id 
  int  ion;             // ionization state (0 = neutral)
  int   ic;             // index of level we ionize to
  int    g;             // statistical weight
  double E;             // excitation energy above ground (in eV)
  double E_ion;         // energy to ionize (in eV)
  double n;             // level population fraction
  double n_lte;         // lte level population
  double b;             // nlte departure coefficient 


  //  ivector photo         // photoionization cross-section vector
  double  P_ic;           // rate of photoionization;

  // photoionization cross-section as a function of wavelength
  xy_array s_photo;
  // recombination coefficient as a function of temperature
  xy_array a_rec;
    
};


class nlte_atom
{

private:


  // matrices, vectors to solve NLTE
  double     **rates;
  gsl_matrix *M_nlte;
  gsl_vector *b_nlte;
  gsl_vector *x_nlte;
  gsl_permutation *p_nlte;

  // functions for NLTE multidimensional solver
  //  int Rate_Equations();
  //void Set_Rates(double N_e, double T, double *J, double egam, double efrac);

  double blackbody_nu(double T, double nu);
  double Calculate_Milne(int lev, double temp);
  void   set_rates(double T, double ne);

public:

  
  int Z;              // Atomic number of atom 

  int n_ions;          // Number of ionic stages considered 
  int n_levels;        // number of energy levels
  int n_lines;         // number of line transitions   
  double n_dens;       // number density of this atom (cm^-3)
  double e_gamma;      // radioactive energy deposited (ergs/sec/cm^3)

  int use_betas;        // include escape probabilites in nlte
  int no_ground_recomb; // suppress recombinations to ground

  // classes
  nlte_level *levels;       // array of level data
  nlte_line  *lines;        // array of line data
  nlte_ion   *ions;         // array of ion data

  std::vector<fuzz_line> fuzz_lines; // vector of fuzz lines
  
  // Constructor and Init
  nlte_atom();
  int Init(std::string, locate_array);
  
  // solve state
  void solve_lte (double T, double ne, double time);
  int  solve_nlte(double T, double ne, double time);
  void print();

  // sobolev
  double get_ion_frac();
  double compute_sobolev_tau(int i, double time);
  void   compute_sobolev_taus(double time);
  
  // returns
  double partition(int ion) 
  {
    for (int i = 0;i < n_ions; i++)
      if (ion == ions[i].stage) return ions[i].part; 
    return -1;
  }
  
  double ion_frac(int ion) 
  {
    for (int i=0;i<n_ions; i++)
      if (ion == ions[i].stage) return ions[i].frac; 
    return 0;
 }
  

 };

#endif