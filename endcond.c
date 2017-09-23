/**
 * @file endcond.c
 * @brief Check for end conditions
 */
#include <math.h>
#include "endcond.h"
#include "particle.h"
#include "simulate.h"
#include "physlib.h"
#include "consts.h"
#include "math.h"

void endcond_check_fo(particle_simd_fo* p_f, particle_simd_fo* p_i, sim_data* sim) {
    int i;

    #pragma omp simd
    for(i = 0; i < NSIMD; i++) {
	if(p_f->running[i]) {

	    /* Max time */
	    if(sim->endcond_active & endcond_tmax) {
		if(p_f->time[i] > sim->endcond_maxSimTime) {
		    p_f->endcond[i] |= endcond_tmax;
		    p_f->running[i] = 0;
		}
	    }

	    /* Wall hit */
	    if(sim->endcond_active & endcond_wall) {
		int tile = wall_hit_wall(p_i->r[i], p_i->phi[i], p_i->z[i],
					 p_f->r[i], p_f->phi[i], p_f->z[i], &sim->wall_data);
		if(tile > 0) {
		    p_f->walltile[i] = tile;
		    p_f->endcond[i] |= endcond_wall;
		    p_f->running[i] = 0;
		}
	    }	

	    /* Min energy */
	    if(sim->endcond_active & endcond_emin) {
		real vnorm = math_normc(p_f->rdot[i], p_f->phidot[i] * p_f->r[i], p_f->zdot[i]);
		real gamma = physlib_relfactorv_fo(vnorm);
		real ekin = CONST_C2 * p_f->mass[i] * (gamma - 1);

		/* Fixed limit */
		if(ekin < sim->endcond_minEkin) {
		    p_f->endcond[i] |= endcond_emin;
		    p_f->running[i] = 0;
		}

		/* Background temperature */
		real Te = plasma_1d_eval_temp(p_f->rho[i], 0, &sim->plasma_data) *CONST_KB;
	        if(ekin < (sim->endcond_minEkinPerTe * Te)) {
		    p_f->endcond[i] |= endcond_therm;
		    p_f->running[i] = 0;
		}
	    }
	    
	    /* Rho lims */
	    if(sim->endcond_active & endcond_rhomax) {
		if(p_f->rho[i] > sim->endcond_maxRho) {
		    p_f->endcond[i] |= endcond_rhomax;
		    p_f->running[i] = 0;
		}
		if(p_f->rho[i] < sim->endcond_minRho) {
		    p_f->endcond[i] |= endcond_rhomin;
		    p_f->running[i] = 0;
		}
	    }

	    /* Orbit lims */
	    if(sim->endcond_active & endcond_polmax) {
		if(fabs(p_f->phi[i]) > sim->endcond_maxTorOrb) {
		    p_f->endcond[i] |= endcond_tormax;
		    p_f->running[i] = 0;
		}
		if(fabs(p_f->pol[i]) > sim->endcond_maxPolOrb) {
		    p_f->endcond[i] |= endcond_polmax;
		    p_f->running[i] = 0;
		}
	    }

	    /* CPU time lim */
	    if(sim->endcond_active & endcond_cpumax) {
		if(p_f->cputime[i] > sim->endcond_maxCpuTime) {
		    p_f->endcond[i] |= endcond_cpumax;
		    p_f->running[i] = 0;
		}
	    }
	}
    }
}

void endcond_check_gc(particle_simd_gc* p_f, particle_simd_gc* p_i, sim_data* sim) {
    int i;

    #pragma omp simd
    for(i = 0; i < NSIMD; i++) {

	/* Max time */
	if(sim->endcond_active & endcond_tmax) {
	    if(p_f->time[i] > sim->endcond_maxSimTime) {
		p_f->endcond[i] |= endcond_tmax;
		p_f->running[i] = 0;
	    }
	}

	/* Wall hit */
	if(sim->endcond_active & endcond_wall) {
	    int tile = wall_hit_wall(p_i->r[i], p_i->phi[i], p_i->z[i],
				     p_f->r[i], p_f->phi[i], p_f->z[i], &sim->wall_data);
	    if(tile > 0) {
		p_f->walltile[i] = tile;
		p_f->endcond[i] |= endcond_wall;
		p_f->running[i] = 0;
	    }
	}	

	/* Min energy */
	if(sim->endcond_active & endcond_emin) {
	    real Bnorm = math_normc(p_f->B_r[i], p_f->B_phi[i], p_f->B_z[i]);
	    real gamma = physlib_relfactorv_gc(p_f->mass[i], p_f->mu[i], p_f->vpar[i], Bnorm);
	    real ekin = CONST_C2 * p_f->mass[i] * (gamma - 1);

	    /* Fixed limit */
	    if(ekin < sim->endcond_minEkin) {
		p_f->endcond[i] |= endcond_emin;
		p_f->running[i] = 0;
	    }

	    /* Background temperature */
	    real Te = plasma_1d_eval_temp(p_f->rho[i], 0, &sim->plasma_data) *CONST_KB;
	    if(ekin < (sim->endcond_minEkinPerTe * Te)) {
		p_f->endcond[i] |= endcond_therm;
		p_f->running[i] = 0;
	    }
	}
	    
	/* Rho lims */
	if(sim->endcond_active & endcond_rhomax) {
	    if(p_f->rho[i] > sim->endcond_maxRho) {
		p_f->endcond[i] |= endcond_rhomax;
		p_f->running[i] = 0;
	    }
	    if(p_f->rho[i] < sim->endcond_minRho) {
		p_f->endcond[i] |= endcond_rhomin;
		p_f->running[i] = 0;
	    }
	}

	/* Orbit lims */
	if(sim->endcond_active & endcond_polmax) {
	    if(fabs(p_f->phi[i]) > sim->endcond_maxTorOrb) {
		p_f->endcond[i] |= endcond_tormax;
		p_f->running[i] = 0;
	    }
	    if(fabs(p_f->pol[i]) > sim->endcond_maxPolOrb) {
		p_f->endcond[i] |= endcond_polmax;
		p_f->running[i] = 0;
	    }
	}

	/* CPU time lim */
	if(sim->endcond_active & endcond_cpumax) {
	    if(p_f->cputime[i] > sim->endcond_maxCpuTime) {
		p_f->endcond[i] |= endcond_cpumax;
		p_f->running[i] = 0;
	    }
	}

	/* Hybrid mode */
	if(sim->sim_mode == 3) {
	    if(p_f->endcond[i] & endcond_rhomax) {
		p_f->endcond[i] |= endcond_hybrid;
	    }
	}
    }
}

void endcond_check_ml(particle_simd_ml* p_f, particle_simd_ml* p_i, sim_data* sim) {
    int i;

    #pragma omp simd
    for(i = 0; i < NSIMD; i++) {

	/* Max time */
	if(sim->endcond_active & endcond_tmax) {
	    if(p_f->time[i] > sim->endcond_maxSimTime) {
		p_f->endcond[i] |= endcond_tmax;
		p_f->running[i] = 0;
	    }
	}

	/* Wall hit */
	if(sim->endcond_active & endcond_wall) {
	    int tile = wall_hit_wall(p_i->r[i], p_i->phi[i], p_i->z[i],
				     p_f->r[i], p_f->phi[i], p_f->z[i], &sim->wall_data);
	    if(tile > 0) {
		p_f->walltile[i] = tile;
		p_f->endcond[i] |= endcond_wall;
		p_f->running[i] = 0;
	    }
	}	
	    
	/* Rho lims */
	if(sim->endcond_active & endcond_rhomax) {
	    if(p_f->rho[i] > sim->endcond_maxRho) {
		p_f->endcond[i] |= endcond_rhomax;
		p_f->running[i] = 0;
	    }
	    if(p_f->rho[i] < sim->endcond_minRho) {
		p_f->endcond[i] |= endcond_rhomin;
		p_f->running[i] = 0;
	    }
	}

	/* Orbit lims */
	if(sim->endcond_active & endcond_polmax) {
	    if(fabs(p_f->phi[i]) > sim->endcond_maxTorOrb) {
		p_f->endcond[i] |= endcond_tormax;
		p_f->running[i] = 0;
	    }
	    if(fabs(p_f->pol[i]) > sim->endcond_maxPolOrb) {
		p_f->endcond[i] |= endcond_polmax;
		p_f->running[i] = 0;
	    }
	}

	/* CPU time lim */
	if(sim->endcond_active & endcond_cpumax) {
	    if(p_f->cputime[i] > sim->endcond_maxCpuTime) {
		p_f->endcond[i] |= endcond_cpumax;
		p_f->running[i] = 0;
	    }
	}
    }
}
