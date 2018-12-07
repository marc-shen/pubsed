#include <math.h>
#include <gsl/gsl_rng.h>
#include "transport.h"
#include "radioactive.h"
#include "physical_constants.h"

namespace pc = physical_constants;


// ------------------------------------------------------
// Propagate a particle using the 
// Implicit Monte Carlo Diffusion (IMD) approach.
// Reference: Gentile, J. of Comput. Physics 172, 543–571 (2001)
// This is only implemented for 1D spherical so far
// ------------------------------------------------------
ParticleFate transport::discrete_diffuse_IMD(particle &p, double dt)
{
  int stop = 0;

  double dx;
  grid->get_zone_size(p.ind,&dx);

  // set this zone
  while (!stop)
  {
    // pointer to current zone
    zone *zone = &(grid->z[p.ind]);

    // add in tally of absorbed and total radiation energy
    #pragma omp atomic
    zone->e_abs += p.e*ddmc_P_abs_[p.ind];
    //zone->e_rad += p.e*ddmc_P_stay_[p.ind];
    #pragma omp atomic
    J_nu_[p.ind][0] += p.e*ddmc_P_stay_[p.ind]*dt*pc::c;
//    std::cout <<  p.ind << " " << p.e*ddmc_P_stay_[p.ind] << "\n";

    // total probability of diffusing in some direction
    double P_diff = ddmc_P_up_[p.ind]  + ddmc_P_dn_[p.ind];
    double P_stay = ddmc_P_abs_[p.ind] + ddmc_P_stay_[p.ind];

    // see if diffuse
    double r1 = rangen.uniform();
    if (r1 < P_diff)
    {
      // find dimension to diffuse in...
      // move up or down in this dimension
      double r2 = rangen.uniform();
      if (r2 < ddmc_P_up_[p.ind]/P_diff)
      {
        p.ind++;
        double rr = p.r();
        p.x[0] += p.x[0]/rr*dx;
        p.x[1] += p.x[1]/rr*dx;
        p.x[2] += p.x[2]/rr*dx;
      }
      else
      {
        p.ind--;
        double rr = p.r();
        p.x[0] -= p.x[0]/rr*dx;
        p.x[1] -= p.x[1]/rr*dx;
        p.x[2] -= p.x[2]/rr*dx;
      }
    }
    // don't diffuse
    else
    {
      // check for absorption
      double r2 = rangen.uniform();
      double f_abs = ddmc_P_abs_[p.ind]/P_stay;
      // see if absorbed
      if (r2 < f_abs) { return absorbed;}

      // advect it
      double zone_vel[3], dvds;
      grid->get_velocity(p.ind,p.x,p.D,zone_vel, &dvds);
      p.x[0] += zone_vel[0]*dt;
      p.x[1] += zone_vel[1]*dt;
      p.x[2] += zone_vel[2]*dt;
      p.ind = grid->get_zone(p.x);

      // adiabatic loses
      //p.e *= (1 - zone->diff_v);
      stop = 1;
    }

    p.t += dt;

    // check for escape
    if (p.ind < 0)         {return absorbed;}
    if (p.ind > grid->n_zones - 1)	  {return escaped; }

  }
  return stopped;

}

// ------------------------------------------------------
// Propagate a particle using the 
// Discrete Diffusion Monte Carlo (DDMC) approach.
// Reference: Densmore+, J. of Comput. Physics 222, 485-503 (2007)
// This is only implemented for 1D spherical also. 
// ------------------------------------------------------
ParticleFate transport::discrete_diffuse_DDMC(particle &p, double dt)
{
  // pointer to current zone
  zone *zone = &(grid->z[p.ind]);
  int nz = grid->n_zones;

  // initialize particle's timestamp
  double dt_remaining = dt;

  // indices of adjacent zones
  int ii = p.ind;
  int ip = ii + 1;
  int im = ii - 1;

  if (ip == nz) ip = ii;
  if (im < 0)   im = 0;

  double dx;
  grid->get_zone_size(ii,&dx);

  double dxp1, dxm1;
  grid->get_zone_size(ip,&dxp1);
  grid->get_zone_size(im,&dxm1);

  // Gathering gas opacity
  double sigma_i   = planck_mean_opacity_[ii];
  double sigma_ip1 = planck_mean_opacity_[ip];
  double sigma_im1 = planck_mean_opacity_[im];

  // Compute left/right leakage opacity
  double sigma_leak_left  = (2.0/3.0/dx) * (1.0 / (sigma_i*dx + sigma_im1*dxm1));
  double sigma_leak_right = (2.0/3.0/dx) * (1.0 / (sigma_i*dx + sigma_ip1*dxp1));
  double sigma_leak_tot   = sigma_leak_left + sigma_leak_right;

  // While loop to sample 
  while (dt_remaining > 0.0)
  {
    double xi = rangen.uniform();

    double d_stay, d_leak;

    // Distance until end of time step
    d_stay = pc::c * dt_remaining;

    // Distance to leakage event
    d_leak = -log(xi) / (sigma_leak_left + sigma_leak_right);

    // Perform the event with a smaller distance
    if (d_stay < d_leak)  // Stay in current zone
    {
      // Tally mean intensity
      #pragma omp atomic
      J_nu_[p.ind][0] += p.e*dt*pc::c;

      // advect it
      double zone_vel[3], dvds;
      grid->get_velocity(p.ind,p.x,p.D,zone_vel, &dvds);
      p.x[0] += zone_vel[0]*dt;
      p.x[1] += zone_vel[1]*dt;
      p.x[2] += zone_vel[2]*dt;
      p.ind = grid->get_zone(p.x);

      dt_remaining = -1.0;
    }
    else // Leak to adjacent zone
    {
      double P_leak_left = sigma_leak_left / sigma_leak_tot;
      double xi2 = rangen.uniform();

      if (xi2 <= P_leak_left)  // leak left
      {
        p.ind--;
        double rr = p.r();
        p.x[0] -= p.x[0]/rr*dx;
        p.x[1] -= p.x[1]/rr*dx;
        p.x[2] -= p.x[2]/rr*dx;
      }
      else // leak right
      {
        p.ind++;
        double rr = p.r();
        p.x[0] += p.x[0]/rr*dx;
        p.x[1] += p.x[1]/rr*dx;
        p.x[2] += p.x[2]/rr*dx;
      }
      
      dt_remaining -= d_leak / pc::c;
    }

    p.t += dt;

    // check for escape
    if (p.ind < 0)         {return absorbed;}
    if (p.ind > grid->n_zones - 1)        {return escaped; }

  }

  return stopped;
}



// ------------------------------------------------------
// Calculate the probabilities of diffusion
// for now this only works in 1D spherical coords
// ------------------------------------------------------
void transport::compute_diffusion_probabilities(double dt)
{
  int nz = grid->n_zones;

  for (int i=0;i<nz;i++)
  {
    double dx;
    grid->get_zone_size(i,&dx);

    // indices of adjacent zones
    int ip = i+1;
    if (ip == nz) ip = i;
    int im = i-1;
    if (im < 0)   im = 0;

    double sigma_p = planck_mean_opacity_[i];

    // diffusion probability in zone and adjacent zones
    double Dj0 = pc::c/(3.0*sigma_p);
    double Djp = pc::c/(3.0*sigma_p);
    double Djm = pc::c/(3.0*sigma_p);

    double Dh_up = 2*dx*(Dj0*Djp)/(Dj0*dx + Djp*dx);
    double Dh_dn = 2*dx*(Dj0*Djm)/(Dj0*dx + Djm*dx);

    ddmc_P_up_[i] = (dt/dx)*(Dh_up/dx);
    ddmc_P_dn_[i] = (dt/dx)*(Dh_dn/dx);
    // boundary condition -- don't diffuse inward at innermost cell
    if (i==0)
      ddmc_P_dn_[i] = 0;

    // advection probability
    ddmc_P_adv_[i] = 0.0;
    ddmc_P_abs_[i] = 0.0; //pc::c*grey_opac*grid->z[i].rho; //*epsilon*grid->z[i0].eps_imc;
    ddmc_P_stay_[i] = 0.0;

    // get normalization
    double norm = 1 + ddmc_P_adv_[i] + ddmc_P_abs_[i];
    norm += ddmc_P_up_[i] +  ddmc_P_dn_[i];
    ddmc_P_adv_[i]  /= norm;
    ddmc_P_abs_[i]  /= norm;
    ddmc_P_up_[i]   /= norm;
    ddmc_P_dn_[i]   /= norm;
    ddmc_P_stay_[i] = 1.0/norm;

    //std::cout <<   ddmc_P_up_[i] << "\t" <<   ddmc_P_dn_[i] << "\t" << ddmc_P_adv_[i] << "\t" << ddmc_P_abs_[i] << "\t" << ddmc_P_stay_[i] << "\n";
  }
}

