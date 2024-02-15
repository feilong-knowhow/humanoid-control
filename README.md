# Humanoid Control

## Build and Start

### OCS2

OCS2 is a huge monorepo; **DO NOT** try to compile the whole repo. You only need to compile `ocs2_legged_robot_ros` and
its dependencies following the step below.

1. You are supposed to clone the OCS2, pinocchio, and hpp-fcl as described in the documentation of OCS2.
   ```bash
   # Clone OCS2
   git clone https://github.com/leggedrobotics/ocs2.git
   # Clone pinocchio
   git clone --recurse-submodules https://github.com/leggedrobotics/pinocchio.git
   # Clone hpp-fcl
   git clone --recurse-submodules https://github.com/leggedrobotics/hpp-fcl.git
   # Clone ocs2_robotic_assets
   git clone https://github.com/leggedrobotics/ocs2_robotic_assets.git
   # Install dependencies
   sudo apt install liburdfdom-dev liboctomap-dev libassimp-dev
   ```
2. Compile the `ocs2_legged_robot_ros` package with [catkin tools](https://catkin-tools.readthedocs.io/en/latest/)
   instead of `catkin_make`. It will take you about ten minutes.
   
   ```bash
   catkin config -DCMAKE_BUILD_TYPE=RelWithDebInfo #important
   catkin build ocs2_legged_robot_ros ocs2_self_collision_visualization
   ```
   Ensure you can command the ANYmal as shown in
   the [document](https://leggedrobotics.github.io/ocs2/robotic_examples.html#legged-robot) and below.
   ![](./README.assets/legged_robot.gif)

### Mujoco

For the latest version of mujoco, it is very easy to install it on python:

```bash
pip3 install mujoco
```

### Getting Start

```bash
catkin config -DCMAKE_BUILD_TYPE=RelWithDebInfo #important
catkin build humanoid_controllers humanoid_legged_description mujoco_sim
# To start simulation with the cheat state estimator.
# Input the gait command, and press SPACE on mujoco simulation window. 
roslaunch humanoid_controllers load_cheat_controller.launch
# To start only the NMPC module and simulate with OCS2 dummy node
roslaunch humanoid_dummy legged_robot_sqp.launch
```

## Framework

### Foot Panner

The foot planner is a switch model provided by OCS2, which switch the contact status between right foot and left foot. The foot planner only plans the z-axis of swinging foot as Cubic Spline. 

***TODO: Add the x-axis and y-axis planner?***

### NMPC

The NMPC part solves the following optimization problems at each cycle through the formulation and solving interfaces
provided by OCS2:
$$
\begin{split}
\begin{cases}
\underset{\mathbf u(.)}{\min} \ \ \phi(\mathbf x(t_I)) + \displaystyle \int_{t_0}^{t_I} l(\mathbf x(t), \mathbf u(t),
t) \, dt \\
\text{s.t.} \ \ \mathbf x(t_0) = \mathbf x_0 \,\hspace{11.5em} \text{initial state} \\
\ \ \ \ \ \dot{\mathbf x}(t) = \mathbf f(\mathbf x(t), \mathbf u(t), t) \hspace{7.5em} \text{system flow map} \\
\ \ \ \ \ \mathbf g_1(\mathbf x(t), \mathbf u(t), t) = \mathbf{0} \hspace{8.5em} \text{state-input equality
constraints} \\
\ \ \ \ \ \mathbf g_2(\mathbf x(t), t) = \mathbf{0} \hspace{10.5em} \text{state-only equality constraints} \\
\ \ \ \ \ \mathbf h(\mathbf x(t), \mathbf u(t), t) \geq \mathbf{0} \hspace{8.5em} \text{inequality constraints}
\end{cases}\end{split}
$$

In this framework, the system state $\mathbf{x}$ and system input $\mathbf{u}$ are defined by  the OCS2 Centroidal Model, eliminating the need for us to manually define them. The definition of $\mathbf{x}$  and $\mathbf{u}$ according to OCS2 is as follows:

$$
\begin{equation} \mathbf{x}= [\mathbf{h}_{com}^T, \mathbf{q}_b^T, \mathbf{q}_j^T]^T,
\mathbf{u} = [\mathbf{f}_c^T, \mathbf{v}_j^T]^T \end{equation}
$$

where $\mathbf{h}_{com} \in \mathbb{R}^6$ is the collection of the normalized centroidal momentum, $\mathbf{q}=[\mathbf{q}_b^T, \mathbf{q}_j^T]^T$ is the positions of the generalized coordinate, which $\mathbf{q}_b^T$ is the 6-dof position of base, $\mathbf{q}_j^T$ is the postion of every joint. In this framework, the dimension of $\mathbf{x}$ is 24.

$\mathbf{f}_c \in \mathbb{R}^{12}$ consists of contact forces at four contact points. In this framework, we define four 3-dof contact points which refer to the toe and heel of left and right feet.  $\mathbf{v}_j$ is the joint velocities.
While the cost function is simply the quadratic cost of tracking the error of all states and the input, the system
dynamics uses centroidal dynamics with the following constraints:

- Friction cone;
- No motion at the standing foot;
- Zero force at the swinging foot;
- The z-axis position of the swinging foot satisfies the gait-generated curve;
- No motion for the roll axis of the foot.

To solve this optimal control problem, a multiple shooting is formulated to transcribe the optimal control problem to a
nonlinear program (NLP) problem, and the NLP problem is solved using Sequential Quadratic Programming (SQP). The QP
subproblem is solved using HPIPM.

### WBC

WBC only considers the current moment. The WBC optimization task is expressed in the following form according to qpOASES:
$$
\begin{array}{lc}
\min _{x} & \frac{1}{2} x^{T} H x+x^{T} g\left(w_{0}\right) \\
\text { s.t. } & \operatorname{lb} A\left(w_{0}\right) \leq A x \leq \mathrm{ub} A\left(w_{0}\right), \\
& \operatorname{lb}\left(w_{0}\right) \leq x \leq \mathrm{ub}\left(w_{0}\right),
\end{array}
$$
Where $x$ is the optimization variable, with a dimension of 42 in this framework, defined as follows:
$$
\begin{equation} x= [\mathbf{a}_{b}^T, \mathbf{a}_j^T, \mathbf{f}_c^T, \mathbf{T}_j^T]^T\end{equation}
$$
where $\mathbf{a}_{b}^T \in \mathbb{R}^6$ is the base acceleration, $\mathbf{a}_j^T \in \mathbb{R}^{12}$ is the joint acceleration, $\mathbf{f}_c^T \in \mathbb{R}^{12}$ is the contact force and $\mathbf{T}_j^T \in \mathbb{R}^{12}$ is the joint torque. 

In weighted WBC, a portion of the tasks serves as weighted costs,  providing the optimization problem's $H$ and $g(w_0)$. Another portion  of the tasks serves as constraints, providing $A$, $lbA$, $ubA$, $lb$,  and $ub$. The definition of the tasks is shown in the following table.

| Type       | Task                                                         |
| ---------- | ------------------------------------------------------------ |
| cost       | Base XY linear acceleration task                             |
| cost       | Base Z position task (using PD controller to estimate the acceleration) |
| cost       | Base Angular task (using PD controller to estimate the acceleration) |
| cost       | Swing leg position task (using PD controller to estimate the acceleration) |
| cost       | Contact force task                                           |
| constraint | Floating base EOM task                                       |
| constraint | Torque limit task                                            |
| constraint | Friction cone task                                           |

### PD controller

Obtain optimized joint positions and joint velocities from MRT (Model  Reference Tracking), obtain joint accelerations and joint torques from  WBC (Whole Body Control), and feed them into a PD controller after  processing. MRT, based on the trajectory optimized by NMPC, obtains the optimized states and inputs at a high frequency.
$$
\mathbf{q}_{des} = \mathbf{q}_{opt}+\frac{1}{2}\mathbf{a}_jt^2 \\
\mathbf{v}_{des} = \mathbf{v}_{opt}+\mathbf{a}_jt\\
\mathbf{T}_{des} = \mathbf{T}_j
$$

### Frequency

| Module         | Frequency |
| -------------- | --------- |
| NMPC           | 100Hz     |
| MRT            | 500Hz     |
| WBC            | 500Hz     |
| PD Controller  | >1000Hz   |
| State Estimate | 500Hz     |

**To ensure the speed of the program, it is crucial that you optimize the code at least to the RelWithDebInfo level. This is highly important!**

