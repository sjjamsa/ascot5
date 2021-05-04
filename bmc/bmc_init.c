#include "bmc_init.h"

#define PI2E0_5 2.50662827463

void buildParticlesWeightsFromProbabilityMatrix(
    real* probabilityMatrix,
    real* weights,
    particle_state* ps,
    int n,
    dist_5D_data* dist,
    wall_2d_data* w2d
) {

    int indexes[32];
    real p_weights[32];
    int hits[32];
    for (int i=0; i<=n; i++) {
        bmc_dist5D_state_indexes(&(ps[i]), indexes, p_weights, hits, &(ps[i]), dist, w2d);


        weights[i] = 0;
        int err = 0;
        for (int j=0; j<32; j++) {
            if (err) continue;
            if (indexes[j] == -1) {
                printf("Warning: Input marker outside mesh id %d r %e phi %e z %e ppar %e\n", ps[i].id, ps[i].r, ps[i].phi, ps[i].z, ps[i].ppar);
                err = 1;
                continue;
            }
            if (probabilityMatrix[indexes[j]] < 0) {
                printf("Warning: probability ill-defined (%f) on index %d Increase velocity mesh size.\n", probabilityMatrix[indexes[j]], indexes[j]);
                probabilityMatrix[indexes[j]] = 0;
            }
            weights[i] = weights[i] + p_weights[j] * probabilityMatrix[indexes[j]];
        }
    }
}

int fmc_init_importance_sampling_from_source_distribution(
        int *n,
        particle_state** ps,
        int n_total,
        sim_offload_data* sim_offload,
        B_field_data* Bdata,
        real* offload_array,
        offload_package* offload_data,
        int importanceSamplingProbability,
        int rk4_subcycles,
        particle_state* input_ps,
        int input_n_ps
    ) {
    // vacate the phase space to find the phase-space points in the mesh
    // suitable for simulating the BMC scheme

    // init sim data
    sim_data sim;
    sim_init(&sim, sim_offload);
    real* ptr = offload_unpack(offload_data, offload_array,
            sim_offload->B_offload_data.offload_array_length);
    B_field_init(&sim.B_data, &sim_offload->B_offload_data, ptr);

    ptr = offload_unpack(offload_data, offload_array,
            sim_offload->E_offload_data.offload_array_length);
    E_field_init(&sim.E_data, &sim_offload->E_offload_data, ptr);

    ptr = offload_unpack(offload_data, offload_array,
            sim_offload->plasma_offload_data.offload_array_length);
    plasma_init(&sim.plasma_data, &sim_offload->plasma_offload_data, ptr);

    ptr = offload_unpack(offload_data, offload_array,
            sim_offload->neutral_offload_data.offload_array_length);
    neutral_init(&sim.neutral_data, &sim_offload->neutral_offload_data, ptr);

    ptr = offload_unpack(offload_data, offload_array,
            sim_offload->wall_offload_data.offload_array_length);
    wall_init(&sim.wall_data, &sim_offload->wall_offload_data, ptr);

    real* distr_array;
    diag_init_offload(&sim_offload->diag_offload_data, &distr_array, 1);
    dist_5D_offload_data dist5D = sim_offload->diag_offload_data.dist5D;
    int dist_length = sim_offload->diag_offload_data.offload_array_length;

    real importanceSamplingWeights[input_n_ps];

    // compute Ekin for each input particle
    real Ekin[input_n_ps];
    for (int i=0; i<input_n_ps; i++) {
        real Brpz[3] = {input_ps[i].B_r, input_ps[i].B_phi, input_ps[i].B_z};
        real Bnorm   = math_norm(Brpz);
        real p = physlib_gc_p( input_ps[i].mass, input_ps[i].mu, input_ps[i].ppar, Bnorm);
        Ekin[i] = physlib_Ekin_pnorm(input_ps[i].mass, p);

        importanceSamplingWeights[i] = Ekin[i];
        importanceSamplingWeights[i] = 1;
    }
    
    if (importanceSamplingProbability) {
        FILE* f_probability = fopen("distr_prob", "rb");
        if (f_probability == NULL) {
            printf("Warning: Cannot open probability matrix file for importance sampling\n");
            abort();
        }
        real weightsFromProbability[input_n_ps];
        real* probabilityMatrix = (real*)malloc(dist_length * sizeof(real));
        fread(probabilityMatrix, sizeof(real), dist_length, f_probability);

            buildParticlesWeightsFromProbabilityMatrix(probabilityMatrix, weightsFromProbability, input_ps, input_n_ps, &dist5D, &sim.wall_data.w2d);

        for (int i=0; i<= input_n_ps; i++) {
            importanceSamplingWeights[i] = importanceSamplingWeights[i] * weightsFromProbability[i];
        }
    }

    real sum = 0;
    for (int i=0; i<input_n_ps; i++) {
       sum += importanceSamplingWeights[i];
    }
    printf("Init initial Importance sampling weights sum %e\n", sum);
    *n = 0;
    int nparticlesHistogram[input_n_ps];
    for (int i=0; i<input_n_ps; i++) {
        nparticlesHistogram[i] = ceil(importanceSamplingWeights[i] / sum * n_total);
        *n += nparticlesHistogram[i];
    }

    *ps = (particle_state *)malloc(*n * sizeof(particle_state));
    particle_state* ps_tmp; // tmp particle

    int i_tot = 0;
    for (int i=0; i<input_n_ps; i++) {
        for (int j=0; j<nparticlesHistogram[i]; ++j) {
            memcpy(*ps + i_tot, &(input_ps[i]), sizeof(particle_state));
            ps_tmp = *ps + i_tot;
            ps_tmp->n_t_subcycles = rk4_subcycles;
            ps_tmp->weight = ps_tmp->weight / nparticlesHistogram[i];
            ps_tmp->weight = ps_tmp->weight * Ekin[i];
            i_tot++;
        }
    }

    printf("Initialized %d %d particles\n", *n, i_tot);

    printf("BMC mesh and markers initialization complete.\n");
    printf("Mesh size: rmin %e\trmax %e\tnr %d\n", dist5D.min_r, dist5D.max_r, dist5D.n_r);
    printf("Mesh size: phimin %e\tphimax %e\tnphi %d\n", dist5D.min_phi, dist5D.max_phi, dist5D.n_phi);
    printf("Mesh size: zmin %e\tzmax %e\tnz %d\n", dist5D.min_z, dist5D.max_z, dist5D.n_z);
    printf("Mesh size: pparamin %e\tpparamax %e\tnppara %d\n", dist5D.min_ppara, dist5D.max_ppara, dist5D.n_ppara);
    printf("Mesh size: pperpmin %e\tpperpmax %e\tnpperp %d\n", dist5D.min_pperp, dist5D.max_pperp, dist5D.n_pperp);
     
    return 0;
}

int fmc_init_importance_sampling_mesh(
        int *n,
        particle_state** ps,
        int** ps_indexes,
        int n_total,
        int use_hermite,
        sim_offload_data* sim_offload,
        B_field_data* Bdata,
        real* offload_array,
        offload_package* offload_data,
        int importanceSamplingProbability,
        int importanceSamplingdensity,
        int importanceSamplingFromParticles,
        real t,
        real m,
        real q,
        int rk4_subcycles,
        particle_state* input_ps,
        int input_n_ps
    ) {
    // vacate the phase space to find the phase-space points in the mesh
    // suitable for simulating the BMC scheme

    // init sim data
    sim_data sim;
    sim_init(&sim, sim_offload);
    real* ptr = offload_unpack(offload_data, offload_array,
            sim_offload->B_offload_data.offload_array_length);
    B_field_init(&sim.B_data, &sim_offload->B_offload_data, ptr);

    ptr = offload_unpack(offload_data, offload_array,
            sim_offload->E_offload_data.offload_array_length);
    E_field_init(&sim.E_data, &sim_offload->E_offload_data, ptr);

    ptr = offload_unpack(offload_data, offload_array,
            sim_offload->plasma_offload_data.offload_array_length);
    plasma_init(&sim.plasma_data, &sim_offload->plasma_offload_data, ptr);

    ptr = offload_unpack(offload_data, offload_array,
            sim_offload->neutral_offload_data.offload_array_length);
    neutral_init(&sim.neutral_data, &sim_offload->neutral_offload_data, ptr);

    ptr = offload_unpack(offload_data, offload_array,
            sim_offload->wall_offload_data.offload_array_length);
    wall_init(&sim.wall_data, &sim_offload->wall_offload_data, ptr);


    real* distr0_array;
    diag_init_offload(&sim_offload->diag_offload_data, &distr0_array, 1);

    real r;
    real phi;
    real z;
    real ppara;
    real pperp;
    real pr;
    real pphi;
    real pz;

    int n_r, n_phi, n_z, n_ppara, n_pperp, n_pr, n_pphi, n_pz;
    real max_r, max_phi, max_z, max_ppara, max_pperp, max_pr, max_pphi, max_pz;
    real min_r, min_phi, min_z, min_ppara, min_pperp, min_pr, min_pphi, min_pz;
    dist_5D_offload_data dist5D = sim_offload->diag_offload_data.dist5D;
    n_r = dist5D.n_r;
    n_phi = dist5D.n_phi;
    n_z = dist5D.n_z;
    n_ppara = dist5D.n_ppara;
    n_pperp = dist5D.n_pperp;
    max_r = dist5D.max_r;
    max_phi = dist5D.max_phi;
    max_z = dist5D.max_z;
    max_ppara = dist5D.max_ppara;
    max_pperp = dist5D.max_pperp;
    min_r = dist5D.min_r;
    min_phi = dist5D.min_phi;
    min_z = dist5D.min_z;
    min_ppara = dist5D.min_ppara;
    min_pperp = dist5D.min_pperp;

    int dist_length = sim_offload->diag_offload_data.offload_array_length;
    real *histogram = (real*)malloc(dist_length * sizeof(real));

    buildImportantSamplingHistogram(dist_length, histogram, &dist5D, &sim.plasma_data, Bdata, importanceSamplingProbability, importanceSamplingdensity, importanceSamplingFromParticles, t, input_ps, input_n_ps, &sim.wall_data.w2d);

    int *nparticlesHistogram = (int*)malloc(dist_length * sizeof(int));

    real sum = 0;
    for (int i=0; i<dist_length; i++) {
       sum += histogram[i];
    }
    printf("Init initial sum %e\n", sum);
    *n = 0;
    for (int i=0; i<dist_length; i++) {
        nparticlesHistogram[i] = ceil(histogram[i] / sum * n_total);
        // if (histogram[i] / sum * n_total > 0) {
        //     printf("hist %d %f %d\n", i, histogram[i] / sum * n_total, nparticlesHistogram[i]);
        // }
        *n += nparticlesHistogram[i];
    }

    *ps = (particle_state *)malloc(*n * sizeof(particle_state));
    *ps_indexes = (int *)malloc(*n * sizeof(int));
    input_particle p_tmp; // tmp particle
    particle_state ps_tmp; // tmp particle

    int i = 0;
    for (int i_r = 0; i_r < n_r; ++i_r) {
        r = (max_r - min_r) * i_r / n_r + min_r;
        for (int i_phi = 0; i_phi < n_phi; ++i_phi) {
            phi = (max_phi - min_phi) * i_phi / n_phi + min_phi;
            for (int i_z = 0; i_z < n_z; ++i_z) {
                z = (max_z - min_z) * i_z / n_z + min_z;

                if (!wall_2d_inside(r, z, &sim.wall_data.w2d)) {
                    continue;
                }

                for (int i_ppara = 0; i_ppara < n_ppara; ++i_ppara) {
                    ppara = (max_ppara - min_ppara) * i_ppara / n_ppara + min_ppara;
                    for (int i_pperp = 0; i_pperp < n_pperp; ++i_pperp) {
                        pperp = (max_pperp - min_pperp) * i_pperp / n_pperp + min_pperp;
                        bmc_5D_to_particle_state(Bdata, r, phi, z, ppara, pperp, t, 0, &ps_tmp, m, q, rk4_subcycles);

                        unsigned long index = dist_5D_index(i_r, i_phi, i_z,
                                i_ppara, i_pperp,
                                0, 0,
                                n_phi, n_z,
                                n_ppara, n_pperp,
                                1, 1);

                        if (!ps_tmp.err) {
                            for (int i_mc=0; i_mc<nparticlesHistogram[index]; ++i_mc) {
                                ps_tmp.id = i;

                                ps_tmp.use_hermite = 0;
                                ps_tmp.hermite_weights = 1./nparticlesHistogram[index];

                                memcpy(*ps + i, &ps_tmp, sizeof(particle_state));
                                (*ps_indexes)[i] = index;
                                i++;
                            }
                        }
                    }
                }
            }
        }
    }



    printf("Initialized %d %d particles\n", *n, i);

    printf("BMC mesh and markers initialization complete.\n");
    printf("Mesh size: rmin %f\trmax %e\tnr %d\n", dist5D.min_r, dist5D.max_r, dist5D.n_r);
    printf("Mesh size: phimin %f\tphimax %e\tnphi %d\n", dist5D.min_phi, dist5D.max_phi, dist5D.n_phi);
    printf("Mesh size: zmin %f\tzmax %e\tnz %d\n", dist5D.min_z, dist5D.max_z, dist5D.n_z);
    printf("Mesh size: pparamin %f\tpparamax %e\tnppara %d\n", dist5D.min_ppara, dist5D.max_ppara, dist5D.n_ppara);
    printf("Mesh size: pperpmin %f\tpperpmax %e\tnpperp %d\n", dist5D.min_pperp, dist5D.max_pperp, dist5D.n_pperp);
     
    return 0;
}

void buildDensityMatrixFromInputParticles(
    real **histogram,
    int dist_length,
    int n_particles,
    particle_state* input_particles,
    dist_5D_offload_data* dist5D,
    wall_2d_data* w2d
) {
    *histogram = (real*)calloc(dist_length, sizeof(real));
    int indexes[32], target_hit[32];
    real weights[32];

    int i_x[5];
    real r, phi, z, ppara, pperp, p, Ekin;
    for (int i=0; i <= n_particles; i++) {
        bmc_dist5D_state_indexes(&input_particles[i], indexes, weights, target_hit, &input_particles[i], dist5D, w2d);
        for (int j=0; j<=32; j++) {
            compute_5d_coordinates_from_hist_index(indexes[j], i_x, &r, &phi, &z, &ppara, &pperp, dist5D);
            if (target_hit[j])
                continue;
            if (!wall_2d_inside(r, z, w2d))
                    continue;

            real p = sqrt(ppara*ppara + pperp*pperp);
            real Ekin = physlib_Ekin_pnorm(input_particles[i].mass, p);
            (*histogram)[indexes[j]] += weights[j] * input_particles[i].weight * Ekin;
        }
    }
}

void buildImportantSamplingHistogram(
    int dist_length,
    real *histogram,
    dist_5D_offload_data* dist5D,
    plasma_data* plasma_data,
    B_field_data* Bdata,
    int importanceSamplingProbability,
    int importanceSamplingdensity,
    int importanceSamplingFromInputParticles,
    real t,
    particle_state* ps,
    int n_ps,
    wall_2d_data* w2d
) {

    real *histogram_probability, *histogram_from_particles;
    FILE *p_probability;
    if (importanceSamplingProbability) {
        p_probability = fopen("distr_prob", "rb");
        if (p_probability == NULL) {
            printf("Warning: Cannot open probability matrix file for importance sampling\n");
            abort();
        }
        histogram_probability = (real*)malloc(dist_length * sizeof(real));
        fread(histogram_probability, sizeof(real), dist_length, p_probability);
    }

    if (importanceSamplingFromInputParticles) {
        buildDensityMatrixFromInputParticles(&histogram_from_particles, dist_length, n_ps, ps, dist5D, w2d);
    }

    real r,phi,z,ppara,pperp, dens[10];
    for (int i=0; i<dist_length; i++) {

        histogram[i] = 1.;
        int i_x[5];
        compute_5d_coordinates_from_hist_index(i, i_x, &r, &phi, &z, &ppara, &pperp, dist5D);

        // avoid to fill velocity boundaries
        if ((i_x[4] == 0) || (i_x[4] == dist5D->n_pperp - 1) || (i_x[2] == 0) || (i_x[2] == dist5D->n_ppara - 1)) {
            histogram[i] = 0;
            continue;
        }

        if (importanceSamplingFromInputParticles) {
            histogram[i] *= histogram_from_particles[i];
        } else if (importanceSamplingdensity) {

            real psi[1], rho[1], B_dB[15];
            B_field_eval_B_dB(B_dB, r, phi, z, t, Bdata);
            B_field_eval_psi(psi, r, phi, z, t, Bdata);
            B_field_eval_rho(rho, psi[0], Bdata);
            plasma_eval_dens(dens, rho[0], r, phi, z, 0, 1, plasma_data);

            // add space distribution from NBI AUG Q8 source
            if ((phi >= 3.67) && (phi <= 4.19)) {
                histogram[i] *= exp (2.61*r -(z-0.093)*(z-0.093) / 0.021);
            }

            // add space distribution from background plasma
            // histogram[i] *= dens[0];

            // add velocity distribution
            // if ((fabs(ppara) <= 6E-20) && (fabs(ppara) >= 0E-20) && (fabs(pperp) <= 6E-20) && (fabs(pperp) >= 0E-30)) {
            //         histogram[i] *= 1;
            // } else {
            //         histogram[i] = 0;
            // }
        } 
        if (importanceSamplingProbability) {
            histogram[i] *= histogram_probability[i];
        } 
    }
}

int bmc_init_particles(
        int mpi_rank,
        int mpi_size,
        int *n,
        particle_state** ps,
        int** ps_indexes,
        int n_per_vertex,
        sim_offload_data* sim_offload,
        B_field_data* Bdata,
        real* offload_array,
        real t,
        real m,
        real q,
        int rk4_subcycles
    ) {
    // vacate the phase space to find the phase-space points in the mesh
    // suitable for simulating the BMC scheme

    // init sim data
    sim_data sim;
    sim_init(&sim, sim_offload);
    
    real* ptr = offload_array + 
            sim_offload->B_offload_data.offload_array_length +
            sim_offload->E_offload_data.offload_array_length +
            sim_offload->plasma_offload_data.offload_array_length +
            sim_offload->neutral_offload_data.offload_array_length;
    wall_init(&sim.wall_data, &sim_offload->wall_offload_data, ptr);

    real r;
    real phi;
    real z;
    real ppara;
    real pperp;
    real pr;
    real pphi;
    real pz;

    int n_r, n_phi, n_z, n_ppara, n_pperp, n_pr, n_pphi, n_pz;
    real max_r, max_phi, max_z, max_ppara, max_pperp, max_pr, max_pphi, max_pz;
    real min_r, min_phi, min_z, min_ppara, min_pperp, min_pr, min_pphi, min_pz;
    dist_5D_offload_data dist5D = sim_offload->diag_offload_data.dist5D;
    n_r = dist5D.n_r;
    n_phi = dist5D.n_phi;
    n_z = dist5D.n_z;
    n_ppara = dist5D.n_ppara;
    n_pperp = dist5D.n_pperp;
    max_r = dist5D.max_r;
    max_phi = dist5D.max_phi;
    max_z = dist5D.max_z;
    max_ppara = dist5D.max_ppara;
    max_pperp = dist5D.max_pperp;
    min_r = dist5D.min_r;
    min_phi = dist5D.min_phi;
    min_z = dist5D.min_z;
    min_ppara = dist5D.min_ppara;
    min_pperp = dist5D.min_pperp;

    if (sim_offload->diag_offload_data.dist5D_collect) {
        printf("5D: n_r %d n_phi %d n_z %d n_vpara %d n_vperp %d n_per_vertex %d\n", n_r, n_phi, n_z, n_ppara, n_pperp, n_per_vertex);
        *n = n_r * n_phi * n_z * (n_ppara ) * (n_pperp ) * n_per_vertex;
    } else {
        printf("6D: n_r %d n_phi %d n_z %d n_vr %d n_vphi %d n_vz %d n_per_vertex %d\n", n_r, n_phi, n_z, n_pr, n_pphi, n_pz, n_per_vertex);
        *n = n_r * n_phi * n_z * n_pr * n_pphi * n_pz * n_per_vertex;
    }

    print_out(VERBOSE_NORMAL, "Mesh size %d.\n", *n / n_per_vertex);

    input_particle p_tmp; // tmp particle
    particle_state ps_tmp; // tmp particle

    // first loop is used only to compute the number of particles needed
    // i is the total number of particles 
    int n_tot = 0;
    for (int i_r = 0; i_r < n_r; ++i_r) {
        r = (max_r - min_r) * i_r / (n_r-1) + min_r;
        for (int i_phi = 0; i_phi < n_phi; ++i_phi) {
            phi = (max_phi - min_phi) * i_phi / fmax(1, n_phi - 1) + min_phi;
            for (int i_z = 0; i_z < n_z; ++i_z) {
                z = (max_z - min_z) * i_z / (n_z-1) + min_z;

                if (!wall_2d_inside(r, z, &sim.wall_data.w2d)) {
                    continue;
                }

                for (int i_ppara = 0; i_ppara < n_ppara; ++i_ppara) {
                    ppara = (max_ppara - min_ppara) * i_ppara / (n_ppara-1) + min_ppara;
                    for (int i_pperp = 0; i_pperp < n_pperp; ++i_pperp) {
                        pperp = (max_pperp - min_pperp) * i_pperp / (n_pperp-1) + min_pperp;
                        bmc_5D_to_particle_state(Bdata, r, phi, z, ppara, pperp, t, n_tot, &ps_tmp, m, q, rk4_subcycles);

                        if (!ps_tmp.err)
                            n_tot += n_per_vertex;
                    }
                }
            }
        }
    }

    // compute number of particles needed for the specific MPI node
    if (mpi_rank == mpi_size - 1) {
        // printf("n_tot mpi_rank n mpisize %d %d %d %d %d %d\n", n_tot, mpi_rank, *n, mpi_size, n_tot - mpi_rank * (*n / mpi_size));
        *n = n_tot - mpi_rank * (n_tot / mpi_size);
    }
    else
        *n = n_tot / mpi_size;
    int start_index = mpi_rank * (n_tot / mpi_size);

    *ps = (particle_state *)malloc(*n * sizeof(particle_state));
    *ps_indexes = (int *)malloc(*n * sizeof(int));

    int i = 0;
    for (int i_r = 0; i_r < n_r; ++i_r) {
        r = (max_r - min_r) * i_r / (n_r-1) + min_r;
        for (int i_phi = 0; i_phi < n_phi; ++i_phi) {
            phi = (max_phi - min_phi) * i_phi / fmax(1, n_phi - 1) + min_phi;
            for (int i_z = 0; i_z < n_z; ++i_z) {
                z = (max_z - min_z) * i_z / (n_z-1) + min_z;

                if (!wall_2d_inside(r, z, &sim.wall_data.w2d)) {
                    continue;
                }

                for (int i_ppara = 0; i_ppara < n_ppara; ++i_ppara) {
                    ppara = (max_ppara - min_ppara) * i_ppara / (n_ppara-1) + min_ppara;
                    for (int i_pperp = 0; i_pperp < n_pperp; ++i_pperp) {
                        pperp = (max_pperp - min_pperp) * i_pperp / (n_pperp-1) + min_pperp;
                        bmc_5D_to_particle_state(Bdata, r, phi, z, ppara, pperp, t, i, &ps_tmp, m, q, rk4_subcycles);

                        unsigned long index = dist_5D_index(i_r, i_phi, i_z,
                                i_ppara, i_pperp,
                                0, 0,
                                n_phi, n_z,
                                n_ppara, n_pperp,
                                1, 1);

                        if (!ps_tmp.err)
                        {
                            for (int i_mc=0; i_mc<n_per_vertex; ++i_mc) {
                                if ((i >= start_index) && (i < start_index + *n)) {
                                    ps_tmp.id = i;

                                    ps_tmp.hermite_weights = 1. / n_per_vertex;
                                    ps_tmp.use_hermite = 0;
                                    memcpy(*ps + i - start_index, &ps_tmp, sizeof(particle_state));
                                    (*ps_indexes)[i - start_index] = index;
                                }
                                i++;
                            }
                        }
                    }
                }
            }
        }
    }
    
    printf("Initialized %d particles in MPI node out of %d total particles\n", *n, i);
    printf("BMC mesh and markers initialization complete.\n");
    printf("Mesh size: rmin %f\trmax %e\tnr %d\n", dist5D.min_r, dist5D.max_r, dist5D.n_r);
    printf("Mesh size: phimin %f\tphimax %e\tnphi %d\n", dist5D.min_phi, dist5D.max_phi, dist5D.n_phi);
    printf("Mesh size: zmin %f\tzmax %e\tnz %d\n", dist5D.min_z, dist5D.max_z, dist5D.n_z);
    printf("Mesh size: pparamin %f\tpparamax %e\tnppara %d\n", dist5D.min_ppara, dist5D.max_ppara, dist5D.n_ppara);
    printf("Mesh size: pperpmin %f\tpperpmax %e\tnpperp %d\n", dist5D.min_pperp, dist5D.max_pperp, dist5D.n_pperp);

    return 0;
}

int fmc_init_importance_sampling(
        int *n,
        particle_state** ps,
        int** ps_indexes,
        int n_total,
        int use_hermite,
        sim_offload_data* sim_offload,
        B_field_data* Bdata,
        real* offload_array,
        real t,
        real m,
        real q,
        int rk4_subcycles
    ) {
    // vacate the phase space to find the phase-space points in the mesh
    // suitable for simulating the BMC scheme

    // init sim data
    sim_data sim;
    sim_init(&sim, sim_offload);
    
    real* ptr = offload_array + 
            sim_offload->B_offload_data.offload_array_length +
            sim_offload->E_offload_data.offload_array_length +
            sim_offload->plasma_offload_data.offload_array_length +
            sim_offload->neutral_offload_data.offload_array_length;
    wall_init(&sim.wall_data, &sim_offload->wall_offload_data, ptr);

    real* distr0_array;
    diag_init_offload(&sim_offload->diag_offload_data, &distr0_array, 1);

    real r;
    real phi;
    real z;
    real ppara;
    real pperp;
    real pr;
    real pphi;
    real pz;
    real r_new;
    real phi_new;
    real z_new;
    real ppara_new;
    real pperp_new;
    real pr_new;
    real pphi_new;
    real pz_new;

    int n_r, n_phi, n_z, n_ppara, n_pperp, n_pr, n_pphi, n_pz;
    real max_r, max_phi, max_z, max_ppara, max_pperp, max_pr, max_pphi, max_pz;
    real min_r, min_phi, min_z, min_ppara, min_pperp, min_pr, min_pphi, min_pz;
    dist_5D_offload_data dist5D = sim_offload->diag_offload_data.dist5D;
    n_r = dist5D.n_r;
    n_phi = dist5D.n_phi;
    n_z = dist5D.n_z;
    n_ppara = dist5D.n_ppara;
    n_pperp = dist5D.n_pperp;
    max_r = dist5D.max_r;
    max_phi = dist5D.max_phi;
    max_z = dist5D.max_z;
    max_ppara = dist5D.max_ppara;
    max_pperp = dist5D.max_pperp;
    min_r = dist5D.min_r;
    min_phi = dist5D.min_phi;
    min_z = dist5D.min_z;
    min_ppara = dist5D.min_ppara;
    min_pperp = dist5D.min_pperp;

    // printf("5D: n_r %d n_phi %d n_z %d n_vpara %d n_vperp %d n_per_vertex %d\n", n_r, n_phi, n_z, n_ppara, n_pperp, n_per_vertex);
    *n = n_total;
    // print_out(VERBOSE_NORMAL, "Mesh size %d.\n", *n / n_per_vertex);

    *ps = (particle_state *)malloc(*n * sizeof(particle_state));
    *ps_indexes = (int *)malloc(*n * sizeof(int));
    input_particle p_tmp; // tmp particle
    particle_state ps_tmp; // tmp particle

    // open precomputed distr file
    FILE *fp;
    fp = fopen("distr_out", "rb");
    int dist_length = sim_offload->diag_offload_data.offload_array_length;
    printf("%d\n", dist_length);
    real *histogram = (real*)malloc(dist_length * sizeof(real));
    fread(histogram, sizeof(real), dist_length, fp);

    int weights[dist_length];
    for (int i=0; i< dist_length; i++) {
        weights[i] = 0;
    }

    unsigned long index_old, index_new;
    int i_x[5], i_x_new[5];
    i_x[0] = n_r / 2;
    i_x[1] = n_phi / 2;
    i_x[2] = n_z / 2;
    i_x[3] = n_ppara / 2;
    i_x[4] = n_pperp / 2;
    // int i_r = n_r / 2, i_phi = n_phi / 2, i_z = n_z / 2;
    // int i_ppara = n_ppara / 2, i_pperp = n_pperp / 2;
    // int i_r_new, i_phi_new, i_z_new, i_ppara_new, i_pperp_new;
    compute_element_5d_coordinates(i_x, &r, &phi, &z, &ppara, &pperp, &dist5D);

    index_old = dist_5D_index(i_x[0], i_x[1], i_x[2],
        i_x[3], i_x[4],
        0, 0,
        n_phi, n_z,
        n_ppara, n_pperp,
        1, 1);

    int accepted = 0;
    int i = 0;
    while (i < *n) { 

        importance_sampling_random_move(i_x, i_x_new, i);
        // printf("%f %f %f %f %f %d %d\n", r, z, dist5D.min_r, dist5D.min_z, dist5D.max_z, i_x_new[2], i_x[2]);
        compute_element_5d_coordinates(i_x_new, &r, &phi, &z, &ppara, &pperp, &dist5D);

        if (!wall_2d_inside(r, z, &sim.wall_data.w2d)) {
            continue;
        }

        printf("%d %d\n", i, *n);

        // while (!wall_2d_inside(r, z, &sim.wall_data.w2d)) {
        //     importance_sampling_random_move(&i_r, &i_phi, &i_z, &i_ppara, &i_pperp);
        //     compute_element_5d_coordinates(i_r, i_phi, i_z, i_ppara, i_pperp, &r_new, &phi_new, &z_new, &ppara_new, &pperp_new);
        // }

        index_new = dist_5D_index(i_x_new[0], i_x_new[1], i_x_new[2],
            i_x_new[3], i_x_new[4],
            0, 0,
            n_phi, n_z,
            n_ppara, n_pperp,
            1, 1);

        double r_rand = r2(); 
        // printf("%d %d %e %e\n", index_new, index_old, histogram[index_new] / histogram[index_old], histogram[index_old]);
        if ((histogram[index_new] / histogram[index_old]) > r_rand ) {
            // printf("pass\n");
            index_old = index_new;
            r = r_new;
            phi = phi_new;
            z = z_new;
            ppara = ppara_new;
            pperp = pperp_new;
            memcpy(i_x, i_x_new, 5*sizeof(int));
        }

        bmc_5D_to_particle_state(Bdata, r, phi, z, ppara, pperp, t, i, &ps_tmp, m, q, rk4_subcycles);

        if (!ps_tmp.err) {
            weights[index_old]++;
            ps_tmp.id = i;
            ps_tmp.use_hermite = 0;
            // ps_tmp.hermite_weights = 1. / n_per_vertex;
            memcpy(*ps + i, &ps_tmp, sizeof(particle_state));
            (*ps_indexes)[i] = index_old;
            i++;
        } else {
            printf("err\n");
        }
    }

    printf("Computing particles weights\n");

    for (int i = 0; i < *n; i++) {
        (*ps)[i].hermite_weights = 1. / weights[(*ps_indexes)[i]];
    }

    printf("Initialized %d particles\n", n);

    return 0;
}

double r2()
{
    return (double)rand() / (double)RAND_MAX ;
}

// Assumes 0 <= max <= RAND_MAX
// Returns in the closed interval [0, max]
long random_at_most(long max) {
  unsigned long
    // max <= RAND_MAX < ULONG_MAX, so this is okay.
    num_bins = (unsigned long) max + 1,
    num_rand = (unsigned long) RAND_MAX + 1,
    bin_size = num_rand / num_bins,
    defect   = num_rand % num_bins;

  long x;
  do {
   x = random();
  }
  // This is carefully written not to overflow
  while (num_rand - defect <= (unsigned long)x);

  // Truncated division is intentional
  return x/bin_size;
}

int init_simd_gc_particles(particle_state* ps, int n_ps, particle_simd_gc** p, B_field_data* Bdata) {
    int n_simd = (n_ps + NSIMD - 1) / NSIMD;
    *p = (particle_simd_gc *)malloc(n_simd * sizeof(particle_simd_gc)); 
    int i_ps = 0, i_simd = 0;
    while (i_ps < n_ps) {
        for (int j = 0; j < NSIMD; j++) {
            if (i_ps < n_ps) {
                particle_state_to_gc(ps + i_ps, i_ps, *p + i_simd, j, Bdata);
                i_ps++;
            } else {
                p[0][i_simd].running[j] = 0;
                p[0][i_simd].id[j] = -1;
            }
        }
        i_simd++;
    }
    return n_simd;
}

void importance_sampling_random_move(int* i_x, int* i_x_new, int i) {
    int dim = i % 5;
    memcpy(i_x_new, i_x, 5 * sizeof(int));
    i_x_new[dim] += random_at_most(4) - 2;
}

void bmc_init_fo_particle(
        input_particle* p,
        real r, real phi, real z,
        real p_r, real p_phi, real p_z,
        real t,
        real m,
        real q,
        int id
    ) {
    p->p.r = r;
    p->p.phi = phi;
    p->p.z = z;
    p->p.p_r = p_r;
    p->p.p_phi = p_phi;
    p->p.p_z = p_z;
    p->p.mass = m;
    p->p.charge = q;
    p->p.anum = 0;
    p->p.znum = 1;
    p->p.weight = 1;
    p->p.time = t;
    p->p.id = id;
    p->type = input_particle_type_p;
}

