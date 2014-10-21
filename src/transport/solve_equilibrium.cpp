#include <stdio.h>
#include <math.h>
#include <iostream>
#include "transport.h"
#include "physical_constants.h"
#include "Lua.h"
#include "grid_general.h"

namespace pc = physical_constants;

//-------------------------------------------------------------
//  Solve for the temperature assuming radiative equilibrium
//-------------------------------------------------------------
void transport::solve_eq_temperature()
{
  // solve radiative equilibrium temperature
  for (int i=0;i<grid->n_zones;i++) 
    grid->z[i].T_gas = temp_brent_method(i);
       
  // mpi reduce the results
  //grid->reduce_T_gas();
}
  

//***************************************************************/
// This is the function that expresses radiative equillibrium
// in a cell (i.e. E_absorbed = E_emitted).  It is used in
// The Brent solver below to determine the temperature such
// that RadEq holds
//**************************************************************/
double transport::rad_eq_function(int c,double T)
{
  // total energy absorbed in zone
  double E_absorbed = grid->z[c].e_abs;
  // total energy emitted (to be calculated)
  double E_emitted = 0.;

  // Calculate total emission assuming no frequency (grey) opacity
  if (nu_grid.size() == 1)
  {
    E_emitted = 4.0*pc::pi*abs_opac[c][0]*pc::sb/pc::pi*pow(T,4);
  }
  // integrate emisison over frequency (angle
  // integration gives the 4*PI) to get total
  // radiation energy emitted. Opacities are
  // held constant for this (assumed not to change
  // much from the last time step).
  else for (int i=0;i<nu_grid.size();i++)
  {
    double dnu  = nu_grid.delta(i);
    double nu   = nu_grid.center(i);
    double B_nu = blackbody_nu(T,nu);
    double kappa_abs = abs_opac[c][i];
    E_emitted += 4.0*pc::pi*kappa_abs*B_nu*dnu;
  }

  //std::cout << E_emitted << " " << E_absorbed << "\n";
  // radiative equillibrium condition: "emission equals absorbtion"
  // return to Brent function to iterate this to zero
  return (E_emitted - E_absorbed);
}


//-----------------------------------------------------------
// Brents method (from Numerical Recipes) to solve 
// non-linear equation for T in rad equillibrium
//-----------------------------------------------------------
// definitions used for temperature solver

#define SIGN(a,b) ((b) >= 0.0 ? fabs(a) : -fabs(a))
double transport::temp_brent_method(int cell)
{  
  double brent_solve_tolerance = 1.0e-2;
  double temp_range_min = 1;
  double temp_range_max = 1.0e12;

  int ITMAX = 100;
  double EPS = 3.0e-8;
  int iter;

  // Initial guesses
  double a=temp_range_min;
  double b=temp_range_max;
  double c=b;
  double d,e,min1,min2;
  double fa=rad_eq_function(cell,a);
  double fb=rad_eq_function(cell,b);
  double fc,p,q,r,s,tol1,xm;
  
  //if ((fa > 0.0 && fb > 0.0) || (fa < 0.0 && fb < 0.0))
  //  printf("Root must be bracketed in zbrent");
  fc=fb;
  for (iter=1;iter<=ITMAX;iter++) {
    if ((fb > 0.0 && fc > 0.0) || (fb < 0.0 && fc < 0.0)) {
      c=a;
      fc=fa;
      e=d=b-a;
    }
    if (fabs(fc) < fabs(fb)) {
      a=b;
      b=c;
      c=a;
      fa=fb;
      fb=fc;
      fc=fa;
    }
    tol1=2.0*EPS*fabs(b)+0.5*brent_solve_tolerance;
    xm=0.5*(c-b);
    if (fabs(xm) <= tol1 || fb == 0.0) return b;
    if (fabs(e) >= tol1 && fabs(fa) > fabs(fb)) {
      s=fb/fa;
      if (a == c) {
         p=2.0*xm*s;
         q=1.0-s;
      } else {
	q=fa/fc;
	r=fb/fc;
	p=s*(2.0*xm*q*(q-r)-(b-a)*(r-1.0));
	q=(q-1.0)*(r-1.0)*(s-1.0);
      }
      if (p > 0.0) q = -q;
      p=fabs(p);
      min1=3.0*xm*q-fabs(tol1*q);
      min2=fabs(e*q);
      if (2.0*p < (min1 < min2 ? min1 : min2)) {
	e=d;
	d=p/q;
      } else {
	d=xm;
	e=d;
      }
    } else {
      d=xm;
      e=d;
    }
    a=b;
    fa=fb;
    if (fabs(d) > tol1)
      b += d;
    else
      b += SIGN(tol1,xm);
    fb=rad_eq_function(cell,b);
  }
  printf("Maximum number of iterations exceeded in zbrent");
  return 0.0;
}

