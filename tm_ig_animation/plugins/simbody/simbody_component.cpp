#include "simbody_component.h"
#include <Simbody.h>

using namespace SimTK;

static uint32_t simbody_component_retain_count = 0;

void simbody_component_init() 
{
    simbody_component_retain_count++;
    if (simbody_component_retain_count > 1) return;

    MultibodySystem system;
    SimbodyMatterSubsystem matter(system);
    GeneralForceSubsystem forces(system);
    Force::Gravity gravity(forces, matter, -YAxis, 9.8);

    // Describe mass and visualization properties for a generic body.
    Body::Rigid bodyInfo(MassProperties(1.0, Vec3(0), UnitInertia(1)));
    bodyInfo.addDecoration(Transform(), DecorativeSphere(0.1));

    // Create the moving (mobilized) bodies of the pendulum.
    MobilizedBody::Pin pendulum1(matter.Ground(), Transform(Vec3(0)),
            bodyInfo, Transform(Vec3(0, 1, 0)));
    MobilizedBody::Pin pendulum2(pendulum1, Transform(Vec3(0)),
            bodyInfo, Transform(Vec3(0, 1, 0)));

    // Initialize the system and state.
    State state = system.realizeTopology();
    pendulum2.setRate(state, 5.0);

    // Simulate for 20 seconds.
    RungeKuttaMersonIntegrator integ(system);
    TimeStepper ts(system, integ);
    ts.initialize(state);
    ts.stepTo(20.0);
}

void simbody_component_destroy() 
{
    simbody_component_retain_count--
    if (simbody_component_retain_count > 0) return;
}
