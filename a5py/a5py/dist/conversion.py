"""
Routines for converting distribution abscissae to other coordinates.

File: conversion.py
"""

import numpy as np
import copy
import scipy.constants as constants
import itertools

from scipy.interpolate import griddata, RectBivariateSpline


def convert_vpavpe_to_Exi(dist, masskg, E_edges=None, xi_edges=None):
    """
    Converts vpa and vpe distribution abscissae to energy and pitch.

    This function operates by looping through all other coordinates except
    vpa and vpe, and at each loop calculates
    f_Exi(E, xi) = f_vpavpe(vpa(E_i, xi_i), vpe(E_i, xi_i)) where E_i and
    xi_i are grid points of the new energy-pitch distribution. Interpolation
    is done bilinearly.

    Energy is in electronvolts and pitch is vpa/(vpa^2 + vpe^2)^0.5. The
    transformation is not relativistic.

    Args:
        dist : dict_like <br>
            A vpa-vpe distribution. May hold other dimensions as well.
        masskg : float <br>
            Mass of the species (required for energy conversion) in kg. Note
            that distribution is assumed to consist of markers with equal mass.
        E_edges : array_like, optional <br>
            Energy grid edges in the new distribution. If not given,
            linspace(0, Emax, 10) will be used where Emax is
            e*0.5*masskg*max(vpa^2, vpe^2).
        xi_edges : array_like, optional <br>
            Pitch grid edges in the new distribution. If not given,
            linspace(-1, 1, 10) will be used.

    Returns:
       Energy-pitch distribution dictionary whose other dimensions are same as
       in input.
    """

    if E_edges is None:
        Emax = (1/constants.e) * 0.5 * masskg \
               * np.maximum( dist["vpa_edges"][-1]*dist["vpa_edges"][-1],
                             dist["vpe_edges"][-1]*dist["vpe_edges"][-1] )
        E_edges = np.linspace(0, Emax, 10)
    if xi_edges is None:
        xi_edges = np.linspace(-1, 1, 10)

    ## Create E-xi distribution ##
    Exidist = copy.deepcopy(dist)

    # Remove vpa and vpe components
    del Exidist["density"]
    Exidist["abscissae"].remove("vpa")
    Exidist["abscissae"].remove("vpe")
    for k in list(Exidist):
        if "vpa" in k or "vpe" in k:
            del Exidist[k]

    # Add E and xi abscissae and initialize a new density
    Exidist["abscissae"].insert(3, "E")
    Exidist["abscissae"].insert(4, "xi")
    abscissae = Exidist["abscissae"]

    Exidist["E"]        = (E_edges[0:-1] + E_edges[1:]) / 2
    Exidist["E_edges"]  = E_edges
    Exidist["n_E"]      = Exidist["E"].size

    Exidist["xi"]       = (xi_edges[0:-1] + xi_edges[1:]) / 2
    Exidist["xi_edges"] = xi_edges
    Exidist["n_xi"]     = Exidist["xi"].size

    dims = []
    for a in abscissae:
        dims.append(Exidist["n_" + a])

    Exidist["density"]  = np.zeros(tuple(dims))

    # Transform E-xi grid to points in (vpa,vpa) space that are used in
    # interpolation.
    xig, Eg = np.meshgrid(Exidist["xi"], Exidist["E"])
    vpag = ( xig * np.sqrt( 2*Eg*constants.e/masskg ) ).ravel()
    vpeg = (np.sqrt(1 - xig*xig) * np.sqrt(2*Eg*constants.e/masskg)).ravel()

    # Coordinate transform Jacobian: dvpa dvpe = |jac| dE dxi
    # Jacobian for transform (vpa, vpe) -> (v, xi) is v / sqrt(1-xi^2)
    # because jac = dvpa / dv  = xi, dvpe / dv  = sqrt(1-xi^2)
    #               dvpa / dxi = v,  dvpe / dxi = -xi v / sqrt(1-xi^2),
    # and the Jacobian for (v, xi) -> (E, xi) is e / (mass*v) when
    # E is in electronvolts. Therefore the combined Jacobian is
    # (e/mass) / sqrt(1-xi*xi).
    jac = (constants.e/masskg) / np.sqrt(1 - xig*xig)

    # Interpolate.
    ranges = []
    for a in dist["abscissae"]:
        if a is not "vpa" and a is not "vpe":
            ranges.append(range(dist["n_" + a]))

    iE   = Exidist["abscissae"].index("E")
    ivpa = dist["abscissae"].index("vpa")
    for itr in itertools.product(*ranges):

        idx = []
        for i in range(0, ivpa):
            #idx.append(slice(itr[i], itr[i]+1, 1))
            idx.append(itr[i])

        idx.append(slice(None))
        idx.append(slice(None))

        for i in range(ivpa, len(dist["abscissae"])-2):
            #idx.append(slice(itr[i], itr[i]+1, 1))
            idx.append(itr[i])

        idx = tuple(idx)

        if dist["vpa"].size == 1 and dist["vpe"].size == 1:
            d = np.ones( (Exidist["n_E"], Exidist["n_xi"]) ) \
                * dist["density"][idx]
        else:
            f = RectBivariateSpline(
                dist["vpa"], dist["vpe"],
                np.squeeze(dist["density"][idx]),
                kx=1, ky=1)
            d = np.reshape(f.ev(vpag, vpeg), (Exidist["n_E"], Exidist["n_xi"]))

        Exidist["density"][idx] = d * jac

    return Exidist
