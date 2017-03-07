#ifndef NLTE_GAS_H
#define NLTE_GAS_H
#include <string>

#include "nlte_atom.h"
#include "locate_array.h"
#include "sedona.h"

// structure to hold a list of line data
struct nlteGlobalLine 
{
  double nu;                 // line center fequency
  double f_osc;              // oscillator strength
  double g1_over_g2;         // ratio of statistical weights
  int    ion_weight;         // mean atomic mass of the ion
  int    iLowerLevel;        // global index of lower level
  int    iUpperLevel;        // global index of upper level
  int    iAtom;
  int    iIndex;
};

class nlte_gas
{
 
 private:

  double ne_brent_method(double,double,double,std::vector<real>);
  double charge_conservation(double,std::vector<real>);

  locate_array nu_grid;
  int verbose;
  int solve_error_;
  std::string atomfile_;

  // global list of all levels of all atoms
  std::vector <int> globalLevelList_atom_;
  std::vector <int> globalLevelList_index_;


 public:

  // global list of all lines of all atoms
  std::vector <nlteGlobalLine> globalLineList_;

  std::vector<double>     mass_frac;  // vector of mass fractions
  std::vector<int>           elem_Z;  // vector of element atomic numbers
  std::vector<int>           elem_A;  // vector of element atomic weights
  std::vector<nlte_atom>      atoms;  // vector of atoms

  double dens;                   // total mass density (g cm^-3)
  double ne;                     // electron number density (cm^-3)
  double temp;                   // Temperature (K)
  double time;                   // Time since Explosion (days) 
  double e_gamma;                // gamma-ray deposited energy
  double A_mu;                   // mean atomic weight of the gas
  int no_ground_recomb;          // suppress ground recombinations

  // flags for what opacities to use
  int use_electron_scattering_opacity;   
  int use_line_expansion_opacity;
  int use_fuzz_expansion_opacity;
  int use_free_free_opacity;
  int use_bound_free_opacity;
  int use_bound_bound_opacity;   
  double line_velocity_width_;

  
  // flags for nlte
  int use_nlte_;

  double grey_opacity_;
  double epsilon_;
  
  //***********************************************************
  // INITIALIZATION
  //***********************************************************

  //----------------------------------------------------------------
  // simple constructor
  //----------------------------------------------------------------
  nlte_gas();
  
  //----------------------------------------------------------------
  // initialize the gas by specifying the atoms that will
  // compose it, along with datafile and freq. array
  // inputs:
  // std::string atomfile: name of atom data file (in hdf5)
  // std::vector<int> e:  vector of atomic numbers
  // std::vector<int> A:  vector of atomic weights (in atomic units)
  // locate_array ng:  locate_array giving the freq. array
  // int: use_nlte: whether to use nlte
  //---------------------------------------------------------------
  void initialize
  (std::string, std::vector<int>, std::vector<int>, locate_array, int);

  //---------------------------------------------------------------
  // initialize: overload above with default is use_nlte = 0
  //---------------------------------------------------------------
  void initialize
  (std::string af, std::vector<int> e, std::vector<int> A, locate_array ng)
  {
    initialize(af,e,A,ng,0);
  }

  //-----------------------------------------------------------
  // read fuzz lines from a file
  // input:
  // std::string fuzzfile: name of hdf5 file with fuzz data
  //-----------------------------------------------------------
  int  read_fuzzfile(std::string fuzzfile);
  
  //-----------------------------------------------------------------
  // Set mass fractions of each element in the gas
  // this function will enforce that the mass fractions are
  // normalized (i.e., add up to 1)
  //
  // input: 
  // std::vector<double> x: vector of mass fractions of each element
  //-----------------------------------------------------------------
  void set_mass_fractions(std::vector<double>);
  
  //-----------------------------------------------------------------
  // get the data for the global line data
  // input: vectors that will be resized and set to line data
  //   std::vector<double> nu         frequncy of lines
  //   std::vector<double> f_osc,     oscillator strength of lines
  //   std::vector<double> g1_over_g2 ratio of statisical weights
  //   std::vector<int> lowerLevel    global index of lower level
  //   std::vector<int> upperLevel)   global index of upper level
  //-----------------------------------------------------------------
  void get_global_line_data(std::vector<double> nu,
			    std::vector<double> f_osc,
			    std::vector<double> g1_over_g2,
			    std::vector<int> iLowerLevel,
			    std::vector<int> iUpperLevel);

  
  //***********************************************************
  // BASIC FUNCTIONALITY
  //***********************************************************

  //-----------------------------------------------------------
  // Solve for the gas state (excitation/ionization)
  // the level populations will be stored internally for
  // further calculations
  // input: a vector of the radiation field J_nu
  // output: a flag for status
  //    == 0 for no error
  //    == 1 root not bracketed in electron density solve
  //    == 2 maximum iterations reached in n_e solve
  //-----------------------------------------------------------  
  int solve_state(std::vector<real>);
  int solve_state();

  //-----------------------------------------------------------
  // return the ionization state, i.e., electron density
  // over total ion number density
  //-----------------------------------------------------------
  double get_ionization_state();

  //-----------------------------------------------------------
  // return the fraction of atoms of index i that are in
  // ionization state j.  
  //-----------------------------------------------------------
  double get_ionization_fraction(int i, int j);


  //-----------------------------------------------------------
  // return the fraction of atoms of index i that are in
  // level state j.  
  //-----------------------------------------------------------
  double get_level_fraction(int i, int j);
  double get_level_departure(int i, int j);


  //-----------------------------------------------------------
  // return the number of total global lines (from all ions)
  //-----------------------------------------------------------
  int get_number_of_lines()
  {
    return globalLineList_.size();
  }

  //-----------------------------------------------------------
  // return a vector listing all of the ordered line nu's
  //-----------------------------------------------------------
  std::vector<double> get_line_frequency_list()
  {
    std::vector<double> nu;
    for (size_t i=0;i<globalLineList_.size();i++)
      nu.push_back(globalLineList_[i].nu);
    return nu;
  }


  //-----------------------------------------------------------
  // return a vector listing all of the line ion masses
  //-----------------------------------------------------------
  std::vector<double> get_line_ion_mass_list()
  {
    std::vector<double> m;
    for (size_t i=0;i<globalLineList_.size();i++)
      m.push_back(elem_A[globalLineList_[i].iAtom]);
    return m;
  }



  //***********************************************************
  // OPACITIES AND EMISSIVITIES
  //***********************************************************
  void computeOpacity(std::vector<double>&, std::vector<double>&, 
		      std::vector<double>&);
  double electron_scattering_opacity();
  void free_free_opacity  (std::vector<double>&, std::vector<double>&);
  void bound_free_opacity (std::vector<double>&, std::vector<double>&);
  void bound_bound_opacity(std::vector<double>&, std::vector<double>&);
  void bound_bound_opacity(int, std::vector<double>&, std::vector<double>&);
  void line_expansion_opacity(std::vector<double>&);
  void fuzz_expansion_opacity(std::vector<double>&);
  void get_line_opacities(std::vector<double>&);
 
  void set_minimum_extinction(double d)
  {
    for (size_t i=0;i<atoms.size();++i) atoms[i].minimum_extinction_ = d;
  }

  void print_properties();
  void print();
};


#endif
