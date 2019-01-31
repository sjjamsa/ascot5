"""
Contains definition of BfieldFrame class.

File: bfieldframe.py
"""
import tkinter
import tkinter.ttk as ttk

import numpy as np

from .plotframe import PlotFrame
import a5py.field.plot as bplot

class BfieldFrame(PlotFrame):
    """
    A frame for plotting magnetic field data.
    """

    def __init__(self, gui, ascotpy):
        """
        Initialize and show default plot.
        """

        super().__init__(gui)
        self._gui     = gui
        self.ascotpy = ascotpy

        self._binxlogchoice = tkinter.IntVar(self)
        self._binxminchoice = tkinter.IntVar(self)
        self._binxmaxchoice = tkinter.IntVar(self)
        self._nbinxchoice   = tkinter.IntVar(self)
        self._binylogchoice = tkinter.IntVar(self)
        self._binyminchoice = tkinter.IntVar(self)
        self._binymaxchoice = tkinter.IntVar(self)
        self._nbinychoice   = tkinter.IntVar(self)

        # Set default values for the variables.
        self._binxminchoice.set(1)
        self._binxmaxchoice.set(20)
        self._nbinxchoice.set(100)
        self._binylogchoice.set(0)
        self._binyminchoice.set(-10)
        self._binymaxchoice.set(10)
        self._nbinychoice.set(100)

        self.ascotpy.ascotpy_init(bfield=True)

        self._show2dpanel()


    def _show2dpanel(self):
        panel = self.get_sidepanel()

        vcmd = (self.register(self._validate),
                '%d', '%i', '%P', '%s', '%S', '%v', '%V', '%W')

        binpanel   = tkinter.Frame(panel)
        xminlabel  = tkinter.Label(binpanel, text="R min ")
        xmaxlabel  = tkinter.Label(binpanel, text="R max ")
        nxlabel    = tkinter.Label(binpanel, text="R nbin")
        minxentry  = tkinter.Entry(binpanel, validate = 'key',
                                   validatecommand = vcmd, width=6,
                                   textvariable=self._binxminchoice)
        maxxentry  = tkinter.Entry(binpanel, validate = 'key',
                                   validatecommand = vcmd, width=6,
                                   textvariable=self._binxmaxchoice)
        nxentry    = tkinter.Entry(binpanel, validate = 'key',
                                   validatecommand = vcmd, width=6,
                                   textvariable=self._nbinxchoice)

        yminlabel   = tkinter.Label(binpanel, text="z min ")
        ymaxlabel   = tkinter.Label(binpanel, text="z max ")
        nylabel     = tkinter.Label(binpanel, text="z nbin")
        minyentry   = tkinter.Entry(binpanel, validate = 'key',
                                    validatecommand = vcmd, width=6,
                                    textvariable=self._binyminchoice)
        maxyentry   = tkinter.Entry(binpanel, validate = 'key',
                                    validatecommand = vcmd, width=6,
                                    textvariable=self._binymaxchoice)
        nyentry     = tkinter.Entry(binpanel, validate = 'key',
                                    validatecommand = vcmd, width=6,
                                    textvariable=self._nbinychoice)

        xminlabel.grid(row=0, column=0)
        minxentry.grid(row=0, column=1)
        xmaxlabel.grid(row=1, column=0)
        maxxentry.grid(row=1, column=1)
        nxlabel.grid(  row=2, column=0)
        nxentry.grid(  row=2, column=1)

        yminlabel.grid(row=0, column=2)
        minyentry.grid(row=0, column=3)
        ymaxlabel.grid(row=1, column=2)
        maxyentry.grid(row=1, column=3)
        nylabel.grid(  row=2, column=2)
        nyentry.grid(  row=2, column=3)

        binpanel.pack()
        tkinter.Button(panel, text="Plot", command=self._plot).pack()

        self._plot()

    def _backtoindex(self):
        self.ascotpy.ascotpy_free(bfield=True)
        super()._backtoindex()


    def _validate(self, action, index, value_if_allowed,
                  prior_value, text, validation_type, trigger_type,
                  widget_name):
        if action == "1":
            if text in "0123456789.-+":
                try:
                    float(value_if_allowed)
                    return True
                except ValueError:
                    return False
            else:
                return False


    def _plot(self, *args):
        """
        Read control states and plot.
        """
        fig = self.get_fig()

        r = np.linspace( float(self._binxminchoice.get()),
                         float(self._binxmaxchoice.get()),
                         float(self._nbinxchoice.get()) )

        z = np.linspace( float(self._binyminchoice.get()),
                         float(self._binymaxchoice.get()),
                         float(self._nbinychoice.get()) )

        R, Z = np.meshgrid(r, z)
        R = R.ravel()
        Z = Z.ravel()
        Phi = np.zeros(R.shape)

        out = self.ascotpy.ascotpy_eval_bfield(R, Phi, Z, evalpsi=True)

        out["psi"] = np.reshape(out["psi"], (r.size, z.size))

        axes = fig.add_subplot(1,1,1)
        bplot.plot(r, z, out["psi"], axes=axes)
        self.draw()
