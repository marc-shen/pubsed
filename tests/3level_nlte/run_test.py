import os
import pylab as py
import compare

def run(pdf,plotup,runcommand):

    ###########################################
    # run the code
    ###########################################
    os.system(runcommand)
     
    ###########################################
    # compare the output
    ###########################################
    compare.compare(pdf)

    ###########################################
    # clean results
    ###########################################
    os.system("rm spectrum_1.dat ray_* gomc")
       
