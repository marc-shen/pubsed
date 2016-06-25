#include <limits>
#include <vector>
#include <algorithm>
#include <iostream>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "locate_array.h"

using namespace std;




//---------------------------------------------------------
// Just allocation the memory for this
//---------------------------------------------------------
void locate_array::init(const int n) 
{
  x.assign(n,0);
}

//---------------------------------------------------------
// Initialize with start, stop and delta
// if start==stop make it a catch-all
//---------------------------------------------------------
void locate_array::init(const double start, const double stop, const double del)
{
  if(start>=stop){
    x.resize(1);
    min = -numeric_limits<double>::infinity();
    x[0] = numeric_limits<double>::infinity();
  }

  else{
    int n = ceil( (stop-start)/del );
    n = max(n,1);
    x.resize(n);
    do_log_interpolate = 0;

    min = start;
    for (int i=0; i<n-1; i++) x[i] = start + (i+1)*del;
    x[n-1] = stop;
  }
}


//---------------------------------------------------------
// Initialize with start, stop and delta
// logarithmic spacing
// if start==stop make it a catch-all
//---------------------------------------------------------
void locate_array::log_init(const double start, const double stop, const double del)
{
  if(start>=stop){
    x.resize(1);
    min = -numeric_limits<double>::infinity();
    x[0] = numeric_limits<double>::infinity();
  }

  else{

    do_log_interpolate = 0;

    std::vector<double> arr;
    min =start;
    double v = start + start*del;
    while (v  < stop)
    {
      arr.push_back(v);
      v = v + v*del;
    }
    arr.push_back(stop);
    int n = arr.size();
    x.resize(n);
    for (int i=0;i<n;i++) x[i] = arr[i];
  }
}

//---------------------------------------------------------
// Initialize with start, stop and n_pts
// if start==stop make it a catch-all
// if n==0 make it a catch-all
//---------------------------------------------------------
void locate_array::init(const double start, const double stop, const int n)
{
  if(start==stop || n==0){
    x.resize(1);
    min = -numeric_limits<double>::infinity();
    x[0] = numeric_limits<double>::infinity();
  }

  else{
    double del = (stop - start)/(double)n;
    x.resize(n);
    do_log_interpolate = 0;

    min = start;
    #pragma omp parallel for
    for (int i=0; i<n-1; i++) x[i] = start + (i+1)*del;
    x[n-1] = stop;
  }
}

//---------------------------------------------------------
// Initialize with passed vector
//---------------------------------------------------------
void locate_array::init(const std::vector<double> a, const double minval)
{
  min = minval;
  do_log_interpolate = 0;
  x.assign(a.begin(), a.end());
}


//---------------------------------------------------------
// copy from another
//---------------------------------------------------------
void locate_array::copy(locate_array l)
{
  min = l.min;
  x.resize(l.size());
  for (int i=0;i<l.size();i++) x[i] = l.x[i];
}


//---------------------------------------------------------
// locate (return closest index below the value)
// if off left side of boundary, returns 0
// if off right side of boundary, returns size
//---------------------------------------------------------
int locate_array::locate(const double xval) const
{
  if (x.size() == 1) return 0;
  // upper_bound returns first element greater than xval
  // values mark bin tops, so this is what we want
  return upper_bound(x.begin(), x.end(), xval) - x.begin();
} 


//---------------------------------------------------------
// Linear Interpolation of a passed array
//---------------------------------------------------------
double locate_array::interpolate_between(const double xval, const int i1, const int i2, const vector<double>& y) const 
{
  if (x.size() == 1) return y[0];
  double slope = (y[i2]-y[i1]) / (x[i2]-x[i1]);
  double yval = y[i1] + slope*(xval - x[i1]);
  return yval;
}


//---------------------------------------------------------
// Log-Log Interpolation of a passed array
//---------------------------------------------------------
double locate_array::log_interpolate_between(const double xval, const int i1, const int i2, const vector<double>& y) const
{
  if (x.size() == 1) return y[0];

  // safeguard against equal opacities
  if(y[i1]==y[i2]) return y[i1];

  // safeguard against nonsensical values
  if(y[i1]<=0 || y[i2]<=0) return interpolate_between(xval, i1, i2, y);

  // do logarithmic interpolation
  double slope = log(y[i2]/y[i1]) / log(x[i2]/x[i1]);
  double logyval = log(y[i1]) + slope*log(xval/x[i1]);
  return exp(logyval);
}


//---------------------------------------------------------
// sample uniformally in zone
//---------------------------------------------------------
double locate_array::sample(const int i, const double rand) const
{
  if (x.size() == 1) return 0;
  if (i == 0) return min    + (x[0] - min   )*rand;
  else return        x[i-1] + (x[i] - x[i-1])*rand;
}

//---------------------------------------------------------
// simple printout
//---------------------------------------------------------
void locate_array::print() const
{
  printf("# Print Locate Array; n_elements = %lu\n",x.size());
  printf("min %12.4e\n",min);
  for (int i=0;i<x.size();i++)
    printf("%4d %12.4e\n",i,x[i]);
}
  

 

//---------------------------------------------------------
// find the value of y at the locate_array's value of xval
// assumes 1-1 correspondence between y and locate_array
// will extrapolate to regions off either end of the
// array grid
//---------------------------------------------------------
double locate_array::value_at_extrapolate(const double xval, const vector<double>& y) const{

  int ind = locate(xval);
  int i1, i2;

  if (x.size() == 1) return y[0];

  if(ind == 0){                // If off left side of grid
    i1 = 0;
    i2 = 1;
  }
  else if(ind < x.size()){    // If within expected region of grid
    i1 = ind - 1;
    i2 = ind;
  }
  else{ //if(ind == x.size()) // If off the right side of the grid
    i1 = x.size() - 2;
    i2 = x.size() - 1;
  }

  if(do_log_interpolate) return log_interpolate_between(xval, i1, i2, y);
  else                   return     interpolate_between(xval, i1, i2, y);
}




//---------------------------------------------------------
// find the value of y at the locate_array's value of xval
// assumes 1-1 correspondence between y and locate_array
// will not extrapolate off of array -- if passed xval
// is out of bounds, will just return the end values
//
// overloaded so that doesn't need to do the locate if
// already known and passed.
//---------------------------------------------------------
double locate_array::value_at(const double xval, const vector<double>& y) const
{
  int ind = locate(xval);
  return value_at(xval,y,ind);
}


double locate_array::value_at(const double xval, const vector<double>& y,int ind) const
{

  int i1, i2;

  if (x.size() == 1) return y[0];

  if(ind == 0){                // If off left side of grid
    return y[0];
  }
  else if(ind < x.size()){    // If within expected region of grid
    i1 = ind - 1;
    i2 = ind;
  }
  else{ //if(ind == x.size()) // If off the right side of the grid
    return y[x.size()-1];
  }

  if(do_log_interpolate) return log_interpolate_between(xval, i1, i2, y);
  else                   return     interpolate_between(xval, i1, i2, y);
}


void locate_array::swap(locate_array new_array){
  // swap the vectors
  x.swap(new_array.x);

  // swap the minimum values
  double min_tmp = min;
  min = new_array.min;
  new_array.min = min_tmp;

  // swap the do_log_interpolate parameters
  int tmp = do_log_interpolate;
  do_log_interpolate = new_array.do_log_interpolate;
  new_array.do_log_interpolate = tmp;
}
