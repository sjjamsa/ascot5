/**
 * @file suzuki.c
 * @brief Suzuki beam-stopping coefficients
 *
 * Based on S Suzuki et al 1998 Plasma Phys. Control. Fusion 40 2097
 */
#include <math.h>
#include <stdlib.h>
#include "ascot5.h"
#include "suzuki.h"

/* Hydrogen fits (Anum=1,2,3) */
/* Low energy 9-100 keV, high energy 100-10000 keV, 0-1e22 1/m^3 */
real A_highE[3][10] = {
{12.7, 1.25, 0.452, 0.0105, 0.547, -0.102, 0.36, -0.0298, -0.0959, 0.00421},
{14.1, 1.11, 0.408, 0.0105, 0.547, -0.0403, 0.345, -0.0288, -0.0971, 0.00474},
{12.7, 1.26, 0.449, 0.0105, 0.547, -0.00577, 0.336, -0.0282, -0.0974, 0.00487}
};
real A_lowE[3][10] = {
{-52.9, -1.36, 0.0719, 0.0137, 0.454, 0.403, -0.22, 0.0666, -0.0677, -0.00148},
{-67.9, -1.22, 0.0814, 0.0139, 0.454, 0.465, -0.273, 0.0751, -0.063, -0.000508},
{-74.2, -1.18, 0.0843, 0.0139, 0.453, 0.491, -0.294, 0.0788, -0.0612, -0.000185}
};

/* Impurity fits */
/* Low energy 9-100 keV, high energy 100-10000 keV */
/* In order B_1_1_1, B_1_1_2, B_1_2_1, B_1_2_2, B_2_1_1 etc. */
integer Z_imp[] = { 2, 6, 6, 4, 8, 7, 3, 5, 26 };
real Zeffmin_imp[] = { 1.0, 1.0, 5.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
real Zeffmax_imp[] = { 2.1, 5.0, 6.0, 4.0, 5.0, 5.0, 3.0, 5.0, 5.0 };
real B_highE[9][12] = {
{
    0.231000,
    0.343000,
    -.185000,
    -.162000E-01,
    0.105000,
    -.703000E-01,
    0.531000E-01,
    0.342000E-02,
    -.838000E-02,
    0.415000E-02,
    -.335000E-02,
    -.221000E-03
},
{
    -.101000E+01,
    -.865000E-02,
    -.124000E+00,
    -.145000E-01,
    0.391000E+00,
    0.161000E-01,
    0.298000E-01,
    0.332000E-02,
    -.248000E-01,
    -.104000E-02,
    -.152000E-02,
    -.189000E-03
},
{
    -.100000E+01,
    -.255000E-01,
    -.125000E+00,
    -.142000E-01,
    0.388000E+00,
    0.206000E-01,
    0.297000E-01,
    0.326000E-02,
    -.246000E-01,
    -.131000E-02,
    -.148000E-02,
    -.180000E-03
},
{
    -.613000E+00,
    0.552000E-01,
    -.167000E+00,
    -.159000E-01,
    0.304000E+00,
    0.154000E-02,
    0.436000E-01,
    0.378000E-02,
    -.201000E-01,
    -.216000E-03,
    -.251000E-02,
    -.227000E-03
},
{
    -.102000E+01,
    -.148000E-01,
    -.674000E-01,
    -.917000E-02,
    0.359000E+00,
    0.143000E-01,
    0.139000E-01,
    0.184000E-02,
    -.209000E-01,
    -.732000E-03,
    -.502000E-03,
    -.949000E-04
},
{
    -.102000E+01,
    -.139000E-01,
    -.979000E-01,
    -.117000E-01,
    0.375000E+00,
    0.156000E-01,
    0.224000E-01,
    0.254000E-02,
    -.226000E-01,
    -.889000E-03,
    -.104000E-02,
    -.139000E-03
},
{
    -.441000E+00,
    0.129000E+00,
    -.170000E+00,
    -.162000E-01,
    0.277000E+00,
    -.156000E-01,
    0.466000E-01,
    0.379000E-02,
    -.193000E-01,
    0.753000E-03,
    -.286000E-02,
    -.239000E-03
},
{
    -.732000E+00,
    0.183000E-01,
    -.155000E+00,
    -.172000E-01,
    0.321000E+00,
    0.946000E-02,
    0.397000E-01,
    0.420000E-02,
    -.204000E-01,
    -.619000E-03,
    -.224000E-02,
    -.254000E-03
},
{
    -.820000E+00,
    -.636000E-02,
    0.542000E-01,
    0.395000E-02,
    0.202000E+00,
    0.806000E-03,
    -.200000E-01,
    -.178000E-02,
    -.610000E-02,
    0.651000E-03,
    0.175000E-02,
    0.146000E-03
}
};

integer Z_imp_lowE[] = { 2, 6, 6, 4, 8, 7, 3, 5, 26 };
real Zeffmin_imp_lowE[] = { 1.0, 1.0, 5.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
real Zeffmax_imp_lowE[] = { 2.1, 5.0, 6.0, 4.0, 5.0, 5.0, 3.0, 5.0, 5.0 };
real B_lowE[9][12] = {
{
    -.792000E+00,
    0.420000E-01,
    0.530000E-01,
    -.139000E-01,
    0.301000E+00,
    -.264000E-01,
    -.299000E-01,
    0.607000E-02,
    0.272000E-03,
    0.611000E-02,
    0.347000E-02,
    -.919000E-03
},
{
    0.161000E+00,
    0.598000E-01,
    -.336000E-02,
    -.426000E-02,
    -.157000E+00,
    -.396000E-01,
    0.460000E-02,
    0.219000E-02,
    0.391000E-01,
    0.711000E-02,
    -.144000E-02,
    -.385000E-03
},
{
    0.158000E+00,
    0.554000E-01,
    -.431000E-02,
    -.335000E-02,
    -.155000E+00,
    -.374000E-01,
    0.537000E-02,
    0.174000E-02,
    0.388000E-01,
    0.683000E-02,
    -.160000E-02,
    -.322000E-03
},
{
    0.112000E+00,
    0.495000E-01,
    0.116000E-01,
    -.286000E-02,
    -.149000E+00,
    -.331000E-01,
    -.426000E-02,
    0.980000E-03,
    0.447000E-01,
    0.652000E-02,
    -.356000E-03,
    -.203000E-03
},
{
    0.111000E+00,
    0.541000E-01,
    -.346000E-03,
    -.368000E-02,
    -.108000E+00,
    -.347000E-01,
    0.193000E-02,
    0.181000E-02,
    0.280000E-01,
    0.604000E-02,
    -.841000E-03,
    -.317000E-03
},
{
    0.139000E+00,
    0.606000E-01,
    -.306000E-02,
    -.455000E-02,
    -.133000E+00,
    -.394000E-01,
    0.399000E-02,
    0.236000E-02,
    0.335000E-01,
    0.690000E-02,
    -.124000E-02,
    -.405000E-03
},
{
    0.112000E+00,
    0.495000E-01,
    0.116000E-01,
    -.286000E-02,
    -.149000E+00,
    -.331000E-01,
    -.426000E-02,
    0.980000E-03,
    0.447000E-01,
    0.652000E-02,
    -.356000E-03,
    -.203000E-03
},
{
    0.122000E+00,
    0.527000E-01,
    -.430000E-03,
    -.318000E-02,
    -.151000E+00,
    -.364000E-01,
    0.343000E-02,
    0.151000E-02,
    0.420000E-01,
    0.692000E-02,
    -.141000E-02,
    -.290000E-03
},
{
    -.110000E-01,
    0.202000E-01,
    0.946000E-03,
    -.409000E-02,
    -.666000E-02,
    -.117000E-01,
    -.236000E-03,
    0.202000E-02,
    0.408000E-02,
    0.185000E-02,
    -.648000E-04,
    -.313000E-03
}
};

real suzuki_sigmav(real E, real ne, real te, integer nion, real* ni,
                   int* Anum, int* Znum) {
    int* ind_H = malloc(nion * sizeof(int));
    int* ind_Z = malloc(nion * sizeof(int));

    /* Separate ions into hydrogen species and impurities */
    int n_H = 0, n_Z = 0;
    real dens_H = 0.0, dens_Z = 0.0;
    real Zeff_sum1 = 0.0, Zeff_sum2 = 0.0;
    for(int i = 0; i < nion; i++) {
        if(Znum[i] == 1) {
            ind_H[n_H] = i;
            dens_H += ni[i];
            n_H++;
        } else {
            ind_Z[n_Z] = i;
            dens_Z += ni[i];
            n_Z++;
        }

        Zeff_sum1 += ni[i] * Znum[i] * Znum[i];
        Zeff_sum2 += ni[i] * Znum[i];
    }
    real Zeff = Zeff_sum1 / Zeff_sum2;

    if(n_H == 0) {
        printf("No hydrogen species in plasma\n");
        return 0.0;
    }

    /* Select low- or high-energy coefficient tables */
    real (*A)[10];
    real (*B)[12];
    if(E >= 9.0 && E < 100.0) {
        A = A_lowE;
        B = B_lowE;
    } else if(E < 10000.0) {
        A = A_highE;
        B = B_highE;
    } else {
        printf("Invalid energy %le keV\n", E);
        return 0.0;
    }

    real logE = log(E);
    real N = ne * 1.0e-19;
    real logN = log(N);
    real U = log(te * 1.0e-3);

    /* Equation 28 */
    real sigma_H = 0.0;
    for(int i = 0; i < n_H; i++) {
        int ind_A = Anum[ind_H[i]]-1;
        sigma_H += ni[ind_H[i]] * (A[ind_A][0] * 1.e-16 / E
                                *(1 + A[ind_A][1]*logE + A[ind_A][2]*logE*logE)
                                *(1 + pow(1 - exp(-A[ind_A][3]*N),A[ind_A][4])
                                  *(A[ind_A][5] + A[ind_A][6]*logE
                                    + A[ind_A][7]*logE*logE))
                                *(1 + A[ind_A][8]*U + A[ind_A][9]*U*U));

    }
    sigma_H /= dens_H;

    /* Equations 26 & 27 */
    real sigma_Z = 0.0;
    for(int i = 0; i < n_Z; i++) {
        int ind_B = -1;
        for(int j = 0; j < 9; j++) {
            if(Z_imp[j] == Znum[ind_Z[i]] && Zeff > Zeffmin_imp[j]
               && Zeff < Zeffmax_imp[j]) {
                ind_B = j;
                break;
            }
        }
        if(ind_B < 0) {
            printf("No plasma fit for species %d\n", ind_Z[i]);
            break;
        }
        sigma_Z += ni[ind_Z[i]] / ne * Znum[ind_Z[i]]
            * (B[ind_B][0]
               + B[ind_B][1] * U
               + B[ind_B][2] * logN
               + B[ind_B][3] * logN * U
               + B[ind_B][4] * logE
               + B[ind_B][5] * logE * U
               + B[ind_B][6] * logE * logN
               + B[ind_B][7] * logE * logN * U
               + B[ind_B][8] * logE * logE
               + B[ind_B][9] * logE * logE * U
               + B[ind_B][10] * logE * logE * logN
               + B[ind_B][11] * logE * logE * logN * U);
    }

    free(ind_H);
    free(ind_Z);

    /* Equation 24 */
    return sigma_H * (1 + (Zeff - 1) * sigma_Z);
}
