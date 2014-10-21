nx   = 100
vmax = 1e9
texp = 20*3600.0*24
temp = 1e4;
rho  = 1e-20
dv   = vmax/nx

fout = open("vacuum.mod","w")

fout.write("1D_sphere SNR\n")
fout.write(str(nx) + "\t" + str(0.0) + "\t" + str(texp) + " 1 \n")
fout.write("1.1\n")

for i  in range(nx):
    v = (i+1.0)*dv
    line = "%10.4e %10.4e %10.4e 1.0\n" % (v,rho,temp)
    fout.write(line)

fout.close()
