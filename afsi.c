/**
 * @file afsi.c
 * @brief ASCOT Fusion Source Integrator AFSI
 */
#define _XOPEN_SOURCE 500 /**< rand48 requires POSIX 1995 standard */

#include <math.h>
#include "ascot5.h"
#include "afsi.h"
#include "consts.h"
#include "random.h"
#include "boschhale.h"
#include "diag/dist_5D.h"

random_data rdata;

void afsi_mc(int react, int n, afsi_data* dist1, afsi_data* dist2, afsi_dist_5D* fusion_dist) {

    random_init(rdata, 1);

    real m1, m2;

    switch(react) {
        case 1: /* DT */
        case 2: /* D-He3 */
            m1 = 3.344e-27;
            m2 = 5.008e-27;
            break;
        case 3: /* DDp */
        case 4: /* DDn */
            m1 = 3.344e-27;
            m2 = 3.344e-27;
            break;
    }

    int n_r, n_phi, n_z;
    if(dist1->type == 1) {
        n_r = dist1->dist_5D->n_r;
        n_phi = dist1->dist_5D->n_phi;
        n_z = dist1->dist_5D->n_z;
    }
    else if(dist1->type == 2) {
        n_r = dist1->dist_thermal->n_r;
        n_phi = dist1->dist_thermal->n_phi;
        n_z = dist1->dist_thermal->n_z;
    }

    #pragma omp parallel for
    for(int iR = 0; iR < n_r; iR++) {
        for(int iphi = 0; iphi < n_phi; iphi++) {
            for(int iz = 0; iz < n_z; iz++) {
                real density1 = afsi_get_density(dist1, iR, iphi, iz);
                real density2 = afsi_get_density(dist2, iR, iphi, iz);

                if(density1 > 0 && density2 > 0) {
                    real* v1x = (real*) malloc(n*sizeof(v1x));
                    real* v1y = (real*) malloc(n*sizeof(v1x));
                    real* v1z = (real*) malloc(n*sizeof(v1x));
                    real* v2x = (real*) malloc(n*sizeof(v1x));
                    real* v2y = (real*) malloc(n*sizeof(v1x));
                    real* v2z = (real*) malloc(n*sizeof(v1x));
                    afsi_create_reaction(m1, m2, n, dist1, dist2, iR, iphi,
                                         iz, v1x, v1y, v1z, v2x, v2y, v2z);

                    real vol = afsi_get_volume(dist1, iR);

                    for(int i = 0; i < n; i++) {
                        real vcom2 = (v1x[i]-v2x[i])*(v1x[i]-v2x[i])
                            + (v1y[i]-v2y[i])*(v1y[i]-v2y[i])
                            + (v1z[i]-v2z[i])*(v1z[i]-v2z[i]);

                        real Ecom = 0.5*(m1*m2)/(m1+m2)*vcom2/1.602e-19;

                        fusion_dist->histogram[iphi*(fusion_dist->n_r*fusion_dist->n_z) + iz*fusion_dist->n_r + iR] +=
                            density1 * density2 * sqrt(vcom2)
                            * boschhale_sigma(react, 1e-3*Ecom)/n*vol;
                    }
                }
                else {
                    fusion_dist->histogram[iphi*(fusion_dist->n_r*fusion_dist->n_z) + iz*fusion_dist->n_r + iR] = 0;
                }
            }
        }
    }
}

real afsi_get_density(afsi_data* dist, int iR, int iphi, int iz) {
    if(dist->type == 1) {
        real vol = afsi_get_volume(dist, iR);

        real density = 0.0;
        for(int ippara = 0; ippara < dist->dist_5D->n_ppara; ippara++) {
            for(int ipperp = 0; ipperp < dist->dist_5D->n_pperp; ipperp++) {
                density += dist->dist_5D->histogram[dist_5D_index(iR, iphi, iz,
                           ippara, ipperp, 0, 0, dist->dist_5D->n_phi, dist->dist_5D->n_z,
                           dist->dist_5D->n_ppara, dist->dist_5D->n_pperp, 1, 1)]/vol;
            }
        }
        return density;
    }

    else if(dist->type == 2) {
        return dist->dist_thermal->density[iR*(dist->dist_thermal->n_phi*dist->dist_thermal->n_z + iphi*dist->dist_thermal->n_z)+iz];
    }

    else {
        return 0;
    }
}

real afsi_get_volume(afsi_data* dist, int iR) {
    real dR, dz;

    if(dist->type == 1) {
        dR = (dist->dist_5D->max_r-dist->dist_5D->min_r)/dist->dist_5D->n_r;
        dz = (dist->dist_5D->max_z-dist->dist_5D->min_z)/dist->dist_5D->n_z;
        return  2*CONST_PI*(dist->dist_5D->min_r + iR * dR + 0.5*dR)*dR*dz;
    }

    else if(dist->type == 2) {
        dR = (dist->dist_thermal->max_r-dist->dist_thermal->min_r)/dist->dist_thermal->n_r;
        dz = (dist->dist_thermal->max_z-dist->dist_thermal->min_z)/dist->dist_thermal->n_z;
        return  2*CONST_PI*(dist->dist_thermal->min_r + iR * dR + 0.5*dR)*dR*dz;
        
    }
}


void afsi_create_reaction(real m1, real m2, int n, afsi_data* dist1, afsi_data* dist2, int iR, int iphi, int iz, real* v1x, real* v1y, real* v1z, real* v2x, real* v2y, real* v2z) {
    real* ppara1 = (real*) malloc(n*sizeof(real));
    real* pperp1 = (real*) malloc(n*sizeof(real));
    real* ppara2 = (real*) malloc(n*sizeof(real));
    real* pperp2 = (real*) malloc(n*sizeof(real));

    if(dist1->type == 1) {
        afsi_sample_5D(dist1->dist_5D, ppara1, pperp1, n, iR, iphi, iz);
    }
    else if(dist1->type == 2) {
        afsi_sample_thermal(dist1->dist_thermal, m1, ppara1, pperp1, n, iR, iphi, iz);
    }

    if(dist2->type == 1) {
        afsi_sample_5D(dist2->dist_5D, ppara2, pperp2, n, iR, iphi, iz);
    }
    else if(dist2->type == 2) {
        afsi_sample_thermal(dist2->dist_thermal, m2, ppara2, pperp2, n, iR, iphi, iz);
    }
    for(int i = 0; i < n; i++) {
        real rx = 2*round(random_uniform(rdata))-1;
        real ry = 2*round(random_uniform(rdata))-1;
        real rz = random_uniform(rdata);
        v1x[i] = rx * pperp1[i]/m1 * sqrt(rz);
        v1y[i] = ry * pperp1[i]/m1 * sqrt(1-rz);
        v1z[i] = ppara1[i]/m1;

        rx = 2*round(random_uniform(rdata))-1;
        ry = 2*round(random_uniform(rdata))-1;
        rz = random_uniform(rdata);
        v2x[i] = rx * pperp2[i]/m2 * sqrt(rz);
        v2y[i] = ry * pperp2[i]/m2 * sqrt(1-rz);
        v2z[i] = ppara2[i]/m2;
    }
}

void afsi_create_reaction_products(int react, int n, real* v1x, real* v1y, real* v1z, real* v2x, real* v2y, real* v2z) {
    
}

void afsi_sample_5D(dist_5D_data* dist, real* ppara, real* pperp, int n,
                    int iR, int iphi, int iz) {
    real* cumdist = (real*) malloc(dist->n_ppara*dist->n_pperp*sizeof(real));

    for(int ippara = 0; ippara < dist->n_ppara; ippara++) {
        for(int ipperp = 0; ipperp < dist->n_pperp; ipperp++) {
            if(ippara == 0 && ipperp == 0) {
                cumdist[0] = dist->histogram[dist_5D_index(iR, iphi, iz,
                    0, 0, 0, 0, dist->n_phi, dist->n_z, dist->n_ppara,
                    dist->n_pperp, 1, 1)];
            } else {
                cumdist[ippara*dist->n_pperp+ipperp] =
                    cumdist[ippara*dist->n_pperp+ipperp-1]
                    + dist->histogram[dist_5D_index(iR, iphi, iz,
                        ippara, ipperp, 0, 0, dist->n_phi, dist->n_z,
                        dist->n_ppara, dist->n_pperp, 1, 1)];
            }
        }
    }

    for(int ippara = 0; ippara < dist->n_ppara; ippara++) {
        for(int ipperp = 0; ipperp < dist->n_pperp; ipperp++) {
            cumdist[ippara*dist->n_pperp+ipperp] /=
                cumdist[dist->n_ppara*dist->n_pperp-1];
        }
    }

    for(int i = 0; i < n; i++) {
        real r = random_uniform(rdata);
        for(int j = 0; j < dist->n_ppara*dist->n_pperp; j++) {
            if(cumdist[j] > r) {
                pperp[i] = dist->min_pperp + j%dist->n_pperp*(dist->max_pperp-dist->min_pperp)/dist->n_pperp;
                ppara[i] = dist->min_ppara + j/dist->n_pperp*(dist->max_ppara-dist->min_ppara)/dist->n_ppara;
                break;
            }
        }
    }
}

void afsi_sample_thermal(afsi_thermal_data* data, real mass, real* ppara,
                        real* pperp, int n, int iR, int iphi, int iz) {
    int ind = iR * (data->n_phi * data->n_z) + iphi * data->n_z + iz;
    real temp = data->temperature[ind];
    real dens = data->temperature[ind];

    for(int i = 0; i < n; i++) {
        real w = 0;
        real r1;
        real r2;
    
        while(w <= 1.0) {
            r1 = random_uniform(rdata);
            r2 = random_uniform(rdata);
            w = sqrt(r1*r1 + r2*r2);
        }

        real r3 = random_uniform(rdata);
        real r4 = random_uniform(rdata);

        real E = -temp * (r1*r1 * log(r3) / w + log(r4));
        
        real absv = sqrt(2*E*1.602e-19/mass);

        r1 = random_uniform(rdata);
        r2 = random_uniform(rdata);

        real theta = 2 * 3.14159 * r1;
        real phi = acos(1 - 2*r2);
        real vx = absv * sin(phi) * cos(theta);
        real vy = absv * sin(phi) * sin(theta);
        real vz = absv * cos(phi);

        ppara[i] = vz * mass;
        pperp[i] = sqrt(vx*vx + vy*vy) * mass;
    }
}

void afsi_test_dist(dist_5D_data* dist1) {
    printf("%d %le %le\n", dist1->n_r, dist1->min_r, dist1->max_r);
    printf("%d %le %le\n", dist1->n_phi, dist1->min_phi, dist1->max_phi);
    printf("%d %le %le\n", dist1->n_z, dist1->min_z, dist1->max_z);
    printf("%d %le %le\n", dist1->n_ppara, dist1->min_ppara, dist1->max_ppara);
    printf("%d %le %le\n", dist1->n_pperp, dist1->min_pperp, dist1->max_pperp);
    printf("%d %le %le\n", dist1->n_time, dist1->min_time, dist1->max_time);
    printf("%d %le %le\n", dist1->n_q, dist1->min_q, dist1->max_q);

    real sum = 0.0;

    for(int i = 0; i < dist1->n_r*dist1->n_phi*dist1->n_z*dist1->n_ppara*dist1->n_pperp*dist1->n_time*dist1->n_q; i++) {
        sum += dist1->histogram[i];
    }

    printf("%le\n", sum);
}

void afsi_test_thermal() {
    afsi_thermal_data data;

    data.n_r = 1;
    data.min_r = 0.1;
    data.max_r = 1;
    data.n_phi = 1;
    data.min_phi = 0;
    data.max_phi = 360;
    data.n_z = 1;
    data.min_z = -1;
    data.max_z = 1;

    real temperature = 1e3;
    real density = 1e19;

    data.temperature = &temperature;
    data.density = &density;

    int n = 100000;
    real* ppara = (real*) malloc(n*sizeof(real));
    real* pperp = (real*) malloc(n*sizeof(real));

    afsi_sample_thermal(&data, 3.343e-27, ppara, pperp, n, 0, 0, 0);

    for(int i = 0; i < n; i++) {
        printf("%le %le\n", ppara[i], pperp[i]);
    }
}
