#include <limits>
#include "nlte_atom.h"
#include "physical_constants.h"
#include <stdlib.h>
#include <stdio.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_multiroots.h>
#include <gsl/gsl_linalg.h>
#include <iostream>

using namespace std;

// ---------------------------------------------------
// For the NLTE problem, we are solving a matrix equation
// M x = b
// where 
//   x is the vector of the level population fractions
//   M is the rate matrix
//   and b is the zero vector assuming statistical equilibrium.
//
// the number density in each level is n_i = x_i*n_tot
// where n_tot is the total number density of the species
//
// Note: one of the rate equations is not independent, 
// so in order for the matrix to be non-singular, we need to 
// make the last equation express number conservation
//   sum_i  x_i = 1
// ---------------------------------------------------

namespace pc = physical_constants;

nlte_atom::nlte_atom()
{
  e_gamma = 0;
  no_ground_recomb = 0;
  use_betas = 0;
}



void nlte_atom::solve_lte(double T, double ne, double time)
{

  // calculate partition functions
  for (int i=0;i<n_ions;i++) ions[i].part = 0;
  for (int i=0;i<n_levels;i++)
  {
    levels[i].n = levels[i].g*exp(-levels[i].E/pc::k_ev/T);
    ions[levels[i].ion].part += levels[i].n;
  }

  // thermal debroglie wavelength, lam_t**3
  double lt = pc::h*pc::h/(2.0*pc::pi*pc::m_e*pc::k*T);
  double fac = 2/ne/pow(lt,1.5);

  // calculate saha ratios
  ions[0].frac = 1.0;
  double norm  = 1.0;
  for (int i=1;i<n_ions;i++) 
  {
    // calculate the ratio of i to i-1
    double saha = exp(-1.0*ions[i-1].chi/pc::k_ev/T);
    saha = saha*(ions[i].part/ions[i-1].part)*fac;
    
    // set relative ionization fraction
    ions[i].frac = saha*ions[i-1].frac;

    // check for ridiculously small numbers
    if (ne < 1e-50) ions[i].frac = 0;
    norm += ions[i].frac;
  }
  // renormalize ionization fractions
  for (int i=0;i<n_ions;i++) ions[i].frac = ions[i].frac/norm;
  
  // calculate level densities (bolztmann factors)
  for (int i=0;i<n_levels;i++)
  {
    double E = levels[i].E;
    int    g = levels[i].g;
    double Z = ions[levels[i].ion].part;
    double f = ions[levels[i].ion].frac;
    levels[i].n = f*g*exp(-E/pc::k_ev/T)/Z;
    levels[i].n_lte = levels[i].n;
    levels[i].b = 1;
  }

}

//-------------------------------------------------------
// integrate up the radiation field over  lines
// to get the line J and over bound-free to get the 
// photoionization rates
//-------------------------------------------------------
void nlte_atom::calculate_radiative_rates(std::vector<real> J_nu, double temp)
{

  // calculate photoionization/recombination rates
  // photoionization is Eq. 3.7 in Rutten, Stellar Atmopsheres
  // recombination is from EQ 3.16 of Rutten, stellar atmospheres
  // recombination rate includes stimulated recombination

  for (int i=0;i<n_levels;i++)
  {
    double R_ion = 0;
    double R_rec = 0;

    double chi = levels[i].E_ion;
    double fac1 = 2/pc::c/pc::c;

    int ic = levels[i].ic;
    if (ic != -1)
    {
      for (int j=1;j<levels[i].s_photo.size();j++)
      {
        double  E    = levels[i].s_photo.x[j];
        double nu    = E*pc::ev_to_ergs/pc::h;
        double  E_0  = levels[i].s_photo.x[j-1];
        double nu_0  = E_0*pc::ev_to_ergs/pc::h;
        double dnu   = (nu - nu_0);
        double nu_m  = 0.5*(nu + nu_0);
        double J     = nu_grid.value_at(nu_m,J_nu);
        double sigma = levels[i].s_photo.y[j];

        double Jterm = sigma*J/(pc::h*nu);
        R_ion += Jterm*dnu;
        R_rec += (sigma*fac1*nu_m*nu_m + Jterm)*exp(-1.0*(E - chi)/pc::k_ev/temp)*dnu;  
//        std::cout << i << " " << nu << " " << sigma << " " << R_rec << "\n";
   
      }
      R_ion = 4*pc::pi*R_ion;
      double lam_t = sqrt(pc::h*pc::h/(2*pc::pi*pc::m_e*pc::k*temp));
      double gl_o_gc = (1.0*levels[i].g)/(1.0*levels[ic].g);
      double saha_fac = lam_t*lam_t*lam_t*gl_o_gc/2.0;
      R_rec = 4*pc::pi*R_rec*saha_fac; 
    }

     // recombination coefficient from Hui and Gnedin 1997
    //  http://adsabs.harvard.edu/abs/1997MNRAS.292...27H
    // are fits to Ferland 1992 for alpha_B
    // debug
    double lam_H  = 2*157807./temp;
    double fact   = pow(1 + pow(lam_H/2.740,0.407),2.242);
    double alpha  = 2.753e-14*pow(lam_H,1.5)/fact;
    R_rec = alpha;


    levels[i].P_ic = R_ion; 
    levels[i].R_ci = R_rec;
  }

  // calculate line J's
  //debug -- hard code line width
  double line_beta = 0.01; //v/c
  double x_max = 5;
  double dx    = 0.05;

  for (int i=0;i<n_lines;i++)
  {
    double nu0 = lines[i].nu;
    double dnu = nu0*line_beta;
    double sum  = 0;
    double J0   = 0;

    double nu_d = line_beta*nu0;
    double gamma = lines[i].A_ul;
    double a_voigt = gamma/4/pc::pi/nu_d;

    for (double x=-1*x_max;x<=x_max;x+=dx)
    {
      double phi = voigt_profile_.getProfile(x,a_voigt);
      double n = nu0 + x*dnu;
      double J1 = nu_grid.value_at(n,J_nu)*phi;
      sum += 0.5*(J1 + J0)*dx;
      J0 = J1;
    }
    lines[i].J = sum; 
  }
}



//-------------------------------------------------------
// Set the rates for all possible transitions
//------------------------------------------------------
void nlte_atom::set_rates(double T, double ne, std::vector<real> J_nu)
{
  // zero out rate matrix
  for (int i=0;i<n_levels;i++)
    for (int j=0;j<n_levels;j++)
      rates[i][j] = 0;

  // ------------------------------------------------
  // radiative bound-bound transitions
  // ------------------------------------------------

  // integrate radiation field in every line
  calculate_radiative_rates(J_nu,T);        

  for (int l=0;l<n_lines;l++)
  {
    int lu     = lines[l].lu;
    int ll     = lines[l].ll;

    // spontaneous dexcitation + stimulated emission
    double R_ul = lines[l].B_ul*lines[l].J + lines[l].A_ul;
    double R_lu = lines[l].B_lu*lines[l].J;

    // add in escape probability suppresion
    if (this->use_betas) 
    {
      std::cout << "HI\n";
      R_ul *= lines[l].beta;
      R_lu *= lines[l].beta; 
    }

    // add into rates
    rates[ll][lu] += R_lu;
    rates[lu][ll] += R_ul;

    //printf("RR %d %d %e\n",ll,lu,R_lu);
    //printf("RR %d %d %e\n",lu,ll,R_ul);
  }

  // ------------------------------------------------
  // non-thermal (radioactive) bound-bound transitions
  // ------------------------------------------------
  double norm = 0;
  for (int l=0;l<n_lines;l++) norm   += lines[l].f_lu;

  for (int l=0;l<n_lines;l++)
  {
    int lu  = lines[l].lu;
    int ll  = lines[l].ll;

    double dE = (levels[lu].E - levels[ll].E)*pc::ev_to_ergs;
    double R_lu = e_gamma/n_dens/dE; //*(lines[l].f_lu/norm);

    if (ll != 0) R_lu = 0;

    // add into rates
    rates[ll][lu] += R_lu;

    //printf("GR %d %d %e %e %e\n",ll,lu,R_lu,e_gamma,dE);
  }


  // ------------------------------------------------
  // collisional bound-bound transitions
  // ------------------------------------------------
  for (int i=0;i<n_levels;i++)
    for (int j=0;j<n_levels;j++)
    {
      // skip transitions to same level
      if (i == j) continue;
      // skip if to another ionization state (not bound-bound)
      if (levels[i].ion != levels[j].ion) continue;

      // level energy difference (in eV)
      double dE = levels[i].E - levels[j].E;
      double zeta = dE/pc::k_ev/T;
      // make sure zeta is positive
      if (zeta < 0) zeta = -1*zeta;

      // rate for downward transition: u --> l
      double C = 2.16*pow(zeta,-1.68)*pow(T,-1.5); // f_ul 
      if (zeta == 0) C = 0;

      // rate if it is a upward transition: l --> u
      if (dE < 0)
      {
      	 // use condition that collision rates give LTE
	       double gl = levels[i].g;
	       double gu = levels[j].g;
	       C = C*gu/gl*exp(-zeta);      
      }

      // add into rates
      rates[i][j] += C;

      //printf("CR: %d %d %e\n",i,j,C);
    }

  // ------------------------------------------------
  // bound-free transitions
  // ------------------------------------------------
  for (int i=0;i<n_levels;i++)
  {
    int ic = levels[i].ic;
    if (ic == -1) continue;

    // ionization potential
    int istage  = levels[i].ion;
    double chi  = ions[istage].chi - levels[i].E;
    double zeta = chi/pc::k_ev/T;

    // collisional ionization rate
    double C_ion = 2.7/zeta/zeta*pow(T,-1.5)*exp(-zeta)*ne;
    rates[i][ic] += C_ion;

    // collisional recombination rate
    int gi = levels[i].g;
    int gc = levels[ic].g;
    double C_rec = 5.59080e-16/zeta/zeta*pow(T,-3)*gi/gc*ne*ne;
    rates[ic][i] += C_rec;

    // radiative recombination rate (debug)
    //double R_rec = ne*levels[i].a_rec.value_at(T);
    //// suppress recombinations to ground  
    //if (no_ground_recomb) if (levels[i].E == 0) R_rec = 0;
    
    // photoionization and radiative recombination
    rates[ic][i] += levels[i].R_ci*ne;
    rates[i][ic] += levels[i].P_ic;

    //printf("pc::pi: %d %d %e %e\n",i,ic,R_rec,levels[i].P_ic);
    //printf("CI: %d %d %e %e\n",i,ic,C_rec,C_ion);
    
  }



  // multiply by rates by lte pop in level coming from
  // (becuase we will solve for depature coeffs)
  for (int i=0;i<n_levels;i++)
      for (int j=0;j<n_levels;j++)
	rates[i][j] *= levels[i].n_lte;

  // print out rates if you so like
  //printf("------- rates ----------\n");
  //for (int i=0;i<n_levels;i++)
  //  for (int j=0;j<n_levels;j++)
   //   printf("%5d %5d %14.5e\n",i,j,rates[i][j]);
   //printf("\n");
   for (int i=0;i<n_levels;i++)
    for (int j=0;j<n_levels;j++)   {
        if (isnan(rates[i][j])) std::cout << "NAN RATE\n";
        if (isinf(rates[i][j])) std::cout << "INF RATE: " << i << " - " << j << "\n"; }
}

int nlte_atom::solve_nlte(double T,double ne,double time, std::vector<real> J_nu)
{
  // initialize with LTE populations
  // this will also calculate line taus and betas
  solve_lte(T,ne,time);

  int max_iter = 100;

  // iterate betas
  for (int iter = 0; iter < max_iter; iter++)
  {
    // Set rates
    set_rates(T,ne,J_nu);

    // zero out matrix and vectors
    gsl_matrix_set_zero(M_nlte);
    gsl_vector_set_zero(b_nlte);
    gsl_vector_set_zero(x_nlte);
    gsl_permutation_init(p_nlte);

    // set up diagonal elements of rate matrix
    for (int i=0;i<n_levels;i++) 
    {
      double Rout = 0.0;
      // don't worry i = j rate should be zero
      for (int j=0;j<n_levels;j++) Rout += rates[i][j];
      Rout = -1*Rout;
      gsl_matrix_set(M_nlte,i,i,Rout);
    }
    
    // set off diagonal elements of rate matrix
    for (int i=0;i<n_levels;i++) 
      for (int j=0;j<n_levels;j++)
	       if (i != j) gsl_matrix_set(M_nlte,i,j,rates[j][i]);
    
    // last row expresses number conservation
    for (int i=0;i<n_levels;i++) 
      gsl_matrix_set(M_nlte,n_levels-1,i,levels[i].n_lte);
    gsl_vector_set(b_nlte,n_levels-1,1.0);

    //printf("----\n");
    //for (int i=0;i<n_levels;i++) 
    //  for (int j=0;j<n_levels;j++)
    //   printf("%5d %5d %14.3e\n",i,j,gsl_matrix_get(M_nlte,i,j));
    // printf("----\n");
    
    // solve matrix
    int status;
    gsl_linalg_LU_decomp(M_nlte, p_nlte, &status);
    gsl_linalg_LU_solve(M_nlte, p_nlte, b_nlte, x_nlte);

    // the x vector should now have the solved level 
    // depature coefficients
    for (int i=0;i<n_levels;i++) 
    {
      double b = gsl_vector_get(x_nlte,i);
      double n_nlte = b*levels[i].n_lte;
      levels[i].n = n_nlte;
      levels[i].b = b;
      //printf("%d %e %e\n",i,levels[i].b,levels[i].n/levels[i].n_lte);
    }

    // set the ionization fraction
    for (int i=0;i<n_ions;i++) ions[i].frac = 0;
    for (int i=0;i<n_levels;i++)
      ions[levels[i].ion].frac += levels[i].n;

    if (!this->use_betas) return 1;

    // see if the betas have converged
    int converged   = 1;
    double beta_tol = 0.1;
    for (int i=0; i<n_lines; i++)
    { 
      double old_beta = lines[i].beta;
      compute_sobolev_tau(i,time);
      double new_beta = lines[i].beta;
      
      if (fabs(old_beta - new_beta)/new_beta > beta_tol) 
	converged = 0;
    }
    if (converged) {return 1; }
    //printf("-----\n");
  }

  printf("# NLTE not converging\n");
  return 0;

}

double nlte_atom::get_ion_frac()
{
  double x = 0;
  for (int i=0;i<n_levels;i++)
    x += levels[i].n*levels[i].ion;
  return x;
}

void nlte_atom::compute_sobolev_taus(double time)
{
  for (int i=0;i<n_lines;i++) compute_sobolev_tau(i,time);
}

double nlte_atom::compute_sobolev_tau(int i, double time)
{
  int ll = lines[i].ll;
  int lu = lines[i].lu;

  double nl = levels[ll].n;
  double nu = levels[lu].n;
  double gl = levels[ll].g;
  double gu = levels[lu].g;

  // check for empty levels
  if (nl < std::numeric_limits<double>::min())
  { 
    lines[i].tau  = 0;
    lines[i].etau = 1;
    lines[i].beta = 1;
    return 0;
  }

  double lam   = pc::c/lines[i].nu;
  double tau   = nl*n_dens*pc::sigma_tot*lines[i].f_lu*time*lam;
  // correction for stimulated emission
  tau = tau*(1 - nu*gl/(nl*gu));

  if (nu*gl > nl*gu) {
    printf("laser regime, line %d, whoops\n",i);
    lines[i].tau  = 0;
    lines[i].etau = 1;
    lines[i].beta = 1;
    return 0; }

  double etau = exp(-tau);
  lines[i].etau = etau;
  lines[i].tau  = tau;
  lines[i].beta = (1-etau)/tau;
  return lines[i].tau;
}

//---------------------------------------------------------
// calculate the bound-free extinction coefficient
// (units cm^{-1}) for all levels
//---------------------------------------------------------
void nlte_atom::bound_free_opacity(std::vector<double>& opac)
{
  int ng = nu_grid.size();

  for (int i=0;i<ng;i++)
  {
    opac[i] = 0;
    double E     = pc::h*nu_grid[i]*pc::ergs_to_ev;
    for (int j=0;j<n_levels;j++)
    {
      // get cross-section (zero if outside threshold)
      double sigma = levels[j].s_photo.value_at_with_zero_edges(E);
      opac[i] += n_dens*sigma*levels[j].n;
    }
  }
    
}

//---------------------------------------------------------
// calculate the bound-free extinction coefficient
// (units cm^{-1}) for all levels
//---------------------------------------------------------
void nlte_atom::bound_bound_opacity(double beta_dop, std::vector<double>& opac)
{
  // zero out array
  for (int i=0;i<opac.size();i++) opac[i] = 0;

  // loop over all lines
  for (int i=0;i<n_lines;i++)
  {
    int ll = lines[i].ll;
    int lu = lines[i].lu;

    double nl = levels[ll].n;
    double nu = levels[lu].n;
    double gl = levels[ll].g;
    double gu = levels[lu].g;
    double nu_0 = lines[i].nu;

    double dnu = beta_dop*nu_0;
    double gamma = lines[i].A_ul;
    double a_voigt = gamma/4/pc::pi/dnu;

    lines[i].tau = pc::sigma_tot;
    double alpha_0 = nl*n_dens*pc::sigma_tot*lines[i].f_lu;
    // correction for stimulated emission
    alpha_0 = alpha_0*(1 - nu*gl/(nl*gu));
    if (alpha_0 < 0) continue;
    if (nl == 0) continue;

    if (alpha_0 < 0) printf("%e %e %e %d %d\n",alpha_0,nl*gu,nu*gl,levels[ll].ion,levels[lu].ion);
    
    // region to add to -- hard code to 10 doppler widths
    double nu_1 = nu_0 - dnu*10;
    double nu_2 = nu_0 + dnu*10;
    int inu1 = nu_grid.locate(nu_1);
    int inu2 = nu_grid.locate(nu_2);

    for (int j = inu1;j<inu2;j++)
    {
      double nu = nu_grid[j];
      double x  = (nu_0 - nu)/dnu;
      double phi = voigt_profile_.getProfile(x,a_voigt)/dnu;
      opac[j] += alpha_0*phi;
    }
    
    //printf("%e %e %e %d %d\n",lines[i].nu,nl,alpha_0,inu1,inu2,nu_grid[inu2]);
    
  } 
}



double nlte_atom::Calculate_Milne(int lev, double temp)
{
  // Maxwell-Bolztmann constants
  double v_MB = sqrt(2*pc::k*temp/pc::m_e);
  double MB_A = 4/sqrt(pc::pi)*pow(v_MB,-3);
  double MB_B = pc::m_e/pc::k/2.0/temp;
  double milne_fac = pow(pc::h/pc::c/pc::m_e,2);

  // starting values
  double sum   = 0;
  double nu_t  = levels[lev].E_ion*pc::ev_to_ergs/pc::h;
  double nu    = nu_t;
  double vel   = 0; 
  double fMB   = 0; 
  double sigma = 0; 
  double coef  = 0; 
  double old_vel  = vel;
  double old_coef = coef;

  // integrate over velocity/frequency
  for (int i=1;i<levels[lev].s_photo.size();i++)
  {
    // recombination cross-section
    double E = levels[lev].s_photo.x[i];
    double S = levels[lev].s_photo.y[i];
    nu       = E*pc::ev_to_ergs/pc::h;
    vel      = sqrt(2*pc::h*(nu - nu_t)/pc::m_e);
    if (nu < nu_t) vel = 0;
    fMB   = MB_A*vel*vel*exp(-MB_B*vel*vel);
    sigma = milne_fac*S*nu*nu/vel/vel;
    coef  = vel*sigma*fMB;

    // integrate
    sum += 0.5*(coef + old_coef)*(vel - old_vel);
    // store old values
    old_vel  = vel;
    old_coef = coef;
  }
  
  // ionize to state
  int ic = levels[lev].ic;
  if (ic == -1) return 0;

  // return value
  //return levels[lev].g/levels[ic].g*sum;
  return (1.0*levels[lev].g)/(1.0*levels[ic].g)*sum;
}


void nlte_atom::print()
{

  cout << "--------------------- ions; n = " << n_ions << " ---------------------\n";
  cout << "# ion \t part \t frac \t chi (eV)\n";
  cout << "#---------------------------------------------------------------\n";

  
  for (int i=0;i<n_ions;i++)
    cout << "#   "<<  ions[i].stage << "\t" << ions[i].part << "\t" << ions[i].frac 
	 << "\t" << ions[i].chi << endl;
 

  cout << "\n";
  cout << "--------------------------------------------------------------------\n";
  cout << "--------------------levels; n = " << n_levels << "------------------------\n";
  cout << "# lev   ion     E_ex        g      pop          b_i       ion_to\n";
  cout << "#---------------------------------------------------------------\n";

  for (int i=0;i<n_levels;i++)
  {
    printf("%5d %4d %12.3e %5d %12.3e %12.3e %5d\n",
	   levels[i].globalID,levels[i].ion,
	   levels[i].E,levels[i].g,levels[i].n,
	   levels[i].b,levels[i].ic);
  }

  printf("\n--- line data\n");

  for (int i=0;i<n_lines;i++)
  {
    printf("%8d %4d %4d %12.3e %12.3e %12.3e %12.3e %12.3e\n",
    	   i,lines[i].ll,lines[i].lu,lines[i].nu,lines[i].f_lu,
    	   lines[i].A_ul,lines[i].B_ul,lines[i].B_lu);
  }

  printf("\n--- line optical depths\n");

  for (int i=0;i<n_lines;i++)
  {
    int    ll = lines[i].ll;
    double nl = levels[ll].n;

    printf("%8d %4d %4d %12.3e %12.3e %12.3e\n",
    	   i,lines[i].ll,lines[i].lu,lines[i].nu,
	   lines[i].tau,nl);
  }


}

//-----------------------------------------------------------------
// calculate planck function in frequency units
//-----------------------------------------------------------------
double nlte_atom::blackbody_nu(double T, double nu)
{
  double zeta = pc::h*nu/pc::k/T;
  return 2.0*nu*nu*nu*pc::h/pc::c/pc::c/(exp(zeta)-1);
}
