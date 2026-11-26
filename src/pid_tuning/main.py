import numpy as np
import matplotlib.pyplot as plt

class PID():
    def __init__(self, kp:float=0, ki:float=0, kd:float=0, targ:float=0) -> None:
        '''
        Initialize PID.

        Parameters:
        -----------
        kp : float = 0
            Proportional gain.
        ki : float = 0
            Integral gain.
        kd : float = 0
            Derivative gain.
        targ : float = 0
            Target value.
        '''
        self.kp = kp
        self.ki = ki
        self.kd = kd

        self.targ = targ

        self.reset()

    def reset(self) -> None:
        '''
        Reset PID.
        '''
        self.last_e = None
        self.integ = 0

    def step(self, x:float, dt:float=0.1) -> float:
        '''
        Take step.

        Parameters:
        -----------
        x : float
            Measured value.
        dt : float
            Change in time.

        Returns:
        --------
        float
            Incremented value
        '''
        # compute error
        error = self.targ - x

        # compute derivative
        if self.last_e:
            deriv = (error - self.last_e) / dt
        else:
            deriv = 0
        self.last_e = error

        # compute integral
        self.integ += error * dt

        # compute pid acceleration
        accel = error * self.kp + self.integ * self.ki + deriv * self.kd

        # apply pid acceleration
        return x + accel * dt

    def __str__(self) -> str:
        '''
        Get string representation.

        Returns
        -------
        str
            String representation of PID.
        '''
        return f'PID(kp={self.kp}, ki={self.ki}, kd={self.kd})'





if __name__ == '__main__':
    # set rates
    KP = 0.002
    KI = 0
    KD = 0.2

    # try a set of targets
    pids = [
        PID(kp=KP, ki=KI, kd=KD, targ=30),
        PID(kp=KP, ki=KI, kd=KD, targ=20),
        PID(kp=KP, ki=KI, kd=KD, targ=15),
        PID(kp=KP, ki=KI, kd=KD, targ=10),
        PID(kp=KP, ki=KI, kd=KD, targ=5)
    ]

    # setup data collection
    T = np.arange(0, 2000, 25)
    X = []
    E = []

    S = []
    SX = []

    # create initial condition
    for i in range(len(pids)):
        X.append([0])
        E.append([0])
    
    t_last = 0
    dn = 1

    # start sampling
    for t in T:
        # update time
        dt = t - t_last
        t_last = t

        if dt > 0:
            # pid step
            for i, pid in enumerate(pids):
                x = X[i][-1]

                for j in range(dn):
                    x = pid.step(x, dt / dn)

                    if i == 0:
                        S.append(t + dt/dn * j)
                        SX.append(x)

                # store data
                X[i].append(x)
                E[i].append(pid.last_e)

    # plot collected data
    for i, pid in enumerate(pids):
        x = np.array(X[i])
        e = np.array(E[i])

        plt.plot(T, x, color=f'C{i}', linestyle='-', label=f'PID[{i}]')
        #plt.plot(T, e, color=f'C{i}', linestyle=':')
        plt.axhline(y=pid.targ, color=f'C{i}', linestyle='--')

    #plt.plot(S, SX, 'r.')
    plt.title('PID Convergence: ' + str(pids[0]))
    plt.xlabel('Time (ms)')
    plt.ylabel('Translation Rate')
    plt.grid(True)

    plt.legend()
    plt.show()