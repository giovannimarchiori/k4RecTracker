// Gaudi
#include "Gaudi/Accumulators/RootHistogram.h"
#include "Gaudi/Histograming/Sink/Utils.h"
#include "Gaudi/Property.h"

// edm4hep
#include "edm4hep/MCParticleCollection.h"
#include "edm4hep/SimTrackerHitCollection.h"
#include "edm4hep/TrackCollection.h"
#include "edm4hep/TrackMCParticleLinkCollection.h"

// marlin
#include <marlinutil/HelixClass_double.h>

// ROOT
#include "TH1D.h"

// k4FWCore
#include "k4FWCore/Consumer.h"

#include "GAUDI_VERSION.h"

#if GAUDI_MAJOR_VERSION < 39
namespace Gaudi::Accumulators {
template <unsigned int ND, atomicity Atomicity = atomicity::full, typename Arithmetic = double>
using StaticHistogram = Gaudi::Accumulators::HistogramingCounterBase<ND, Atomicity, Arithmetic, naming::histogramString,
                                                                     HistogramingAccumulator>;
}
#endif

#include <string>

/** @class PlotTrackHitDistances
 *
 *  Gaudi consumer that generates a residual distribution (mm) by comparing the helix from Track AtIP and simHit
 * position. This is intended to be used on tracks produced from gen particles i.e. which do not have real hits attached
 * to them.
 *
 *  @author Brieuc Francois
 */

struct PlotTrackHitDistances final
    : k4FWCore::Consumer<void(const edm4hep::SimTrackerHitCollection&, const edm4hep::TrackMCParticleLinkCollection&)> {
  PlotTrackHitDistances(const std::string& name, ISvcLocator* svcLoc)
      : Consumer(name, svcLoc,
                 {
                     KeyValues("InputSimTrackerHits", {"DCHCollection"}),
                     KeyValues("InputTracksFromGenParticlesAssociation", {"TracksFromGenParticlesAssociation"}),
                 }) {}

  void operator()(const edm4hep::SimTrackerHitCollection& simTrackerHits,
                  const edm4hep::TrackMCParticleLinkCollection& trackParticleAssociations) const override {

    for (const auto& trackParticleAssociation : trackParticleAssociations) {
      auto genParticle = trackParticleAssociation.getTo();
      auto track = trackParticleAssociation.getFrom();
      edm4hep::TrackState trackStateAtIP;
      bool found_trackStateAtIP = false;
      for (const auto& trackState : track.getTrackStates()) {
        if (trackState.location == edm4hep::TrackState::AtIP) {
          trackStateAtIP = trackState;
          found_trackStateAtIP = true;
          break;
        }
      }
      if (!found_trackStateAtIP)
        throw std::runtime_error("No track state defined AtIP, exiting!");

      // Build an helix out of the trackState
      auto helixFromTrack = HelixClass_double();
      helixFromTrack.Initialize_Canonical(trackStateAtIP.phi, trackStateAtIP.D0, trackStateAtIP.Z0,
                                          trackStateAtIP.omega, trackStateAtIP.tanLambda, m_Bz);

      // Fill the histogram with residuals for hits attached to the same gen particle
      for (const auto& simTrackerHit : simTrackerHits) {
        auto simTrackerHitgenParticle = simTrackerHit.getParticle();
        if (simTrackerHitgenParticle.getObjectID() == genParticle.getObjectID()) {
          double simTrackerHitPosition[] = {simTrackerHit.x(), simTrackerHit.y(), simTrackerHit.z()};
          double distances[3];
          helixFromTrack.getDistanceToPoint(simTrackerHitPosition, distances);
          // Distance[0] - distance in R-Phi plane, Distance[1] - distance along Z axis, Distance[2] - 3D distance
          ++m_residualHist[distances[2]];
        }
      }
    }
    return;
  }
  Gaudi::Property<float> m_Bz{this, "Bz", 2., "Z component of the (assumed constant) magnetic field in Tesla."};
  mutable Gaudi::Accumulators::StaticHistogram<1> m_residualHist{
      this, "track_hits_distance_closest_approach", "Track-hit Distances", {100, 0, 1, "Distance [mm];Entries"}};
};

DECLARE_COMPONENT(PlotTrackHitDistances)
