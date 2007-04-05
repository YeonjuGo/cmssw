/** \class EcalWeightUncalibRecHitProducer
 *   produce ECAL uncalibrated rechits from dataframes
 *
  *  $Id: EcalWeightUncalibRecHitProducer.cc,v 1.20 2007/03/09 10:15:04 meridian Exp $
  *  $Date: 2007/03/09 10:15:04 $
  *  $Revision: 1.20 $
  *  \author Shahram Rahatlou, University of Rome & INFN, Sept 2005
  *
  */
#include "RecoLocalCalo/EcalRecProducers/interface/EcalWeightUncalibRecHitProducer.h"
#include "DataFormats/EcalDigi/interface/EcalDigiCollections.h"
#include "DataFormats/EcalDigi/interface/EcalMGPASample.h"
#include "DataFormats/Common/interface/Handle.h"

#include <iostream>
#include <iomanip>
#include <cmath>

#include "FWCore/Framework/interface/ESHandle.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "CondFormats/EcalObjects/interface/EcalPedestals.h"
#include "CondFormats/DataRecord/interface/EcalPedestalsRcd.h"

#include "DataFormats/EcalRecHit/interface/EcalUncalibratedRecHit.h"
#include "DataFormats/EcalRecHit/interface/EcalRecHitCollections.h"

#include "CondFormats/EcalObjects/interface/EcalXtalGroupId.h"
#include "CondFormats/EcalObjects/interface/EcalWeightXtalGroups.h"
#include "CondFormats/DataRecord/interface/EcalWeightXtalGroupsRcd.h"

#include "CondFormats/EcalObjects/interface/EcalWeight.h"
#include "CondFormats/EcalObjects/interface/EcalWeightSet.h"
#include "CondFormats/EcalObjects/interface/EcalTBWeights.h"
#include "CondFormats/DataRecord/interface/EcalTBWeightsRcd.h"

#include "CondFormats/EcalObjects/interface/EcalMGPAGainRatio.h"
#include "CondFormats/EcalObjects/interface/EcalGainRatios.h"
#include "CondFormats/DataRecord/interface/EcalGainRatiosRcd.h"

#include "Math/SVector.h"
#include "Math/SMatrix.h"
#include <vector>

EcalWeightUncalibRecHitProducer::EcalWeightUncalibRecHitProducer(const edm::ParameterSet& ps) {

   EBdigiCollection_ = ps.getParameter<edm::InputTag>("EBdigiCollection");
   EEdigiCollection_ = ps.getParameter<edm::InputTag>("EEdigiCollection");
   EBhitCollection_  = ps.getParameter<std::string>("EBhitCollection");
   EEhitCollection_  = ps.getParameter<std::string>("EEhitCollection");
   produces< EBUncalibratedRecHitCollection >(EBhitCollection_);
   produces< EEUncalibratedRecHitCollection >(EEhitCollection_);
}

EcalWeightUncalibRecHitProducer::~EcalWeightUncalibRecHitProducer() {
}

void
EcalWeightUncalibRecHitProducer::produce(edm::Event& evt, const edm::EventSetup& es) {

   using namespace edm;

   Handle< EBDigiCollection > pEBDigis;
   Handle< EEDigiCollection > pEEDigis;

   const EBDigiCollection* EBdigis =0;
   const EEDigiCollection* EEdigis =0;

   try {
     evt.getByLabel( EBdigiCollection_, pEBDigis);
     //evt.getByLabel( digiProducer_, pEBDigis);
     EBdigis = pEBDigis.product(); // get a ptr to the produc
     edm::LogInfo("EcalUncalibRecHitInfo") << "total # EBdigis: " << EBdigis->size() ;
   } catch (...) {
     // edm::LogError("EcalUncalibRecHitError") << "Error! can't get the product " << EBdigiCollection_.c_str() ;
   }

   try {
     evt.getByLabel( EEdigiCollection_, pEEDigis);
     //evt.getByLabel( digiProducer_, pEEDigis);
     EEdigis = pEEDigis.product(); // get a ptr to the product
     edm::LogInfo("EcalUncalibRecHitInfo") << "total # EEdigis: " << EEdigis->size() ;
   } catch (...) {
     //edm::LogError("EcalUncalibRecHitError") << "Error! can't get the product " << EEdigiCollection_.c_str() ;
   }

    // fetch map of groups of xtals
#ifdef DEBUG
   LogDebug("EcalUncalibRecHitDebug") << "fetching gainRatios....";
#endif
    edm::ESHandle<EcalWeightXtalGroups> pGrp;
    es.get<EcalWeightXtalGroupsRcd>().get(pGrp);
    const EcalWeightXtalGroups* grp = pGrp.product();
    const EcalWeightXtalGroups::EcalXtalGroupsMap& grpMap = grp->getMap();
   // Gain Ratios
   edm::ESHandle<EcalGainRatios> pRatio;
   es.get<EcalGainRatiosRcd>().get(pRatio);
   const EcalGainRatios::EcalGainRatioMap& gainMap = pRatio.product()->getMap(); // map of gain ratios

   // fetch TB weights
#ifdef DEBUG
   LogDebug("EcalUncalibRecHitDebug") <<"Fetching EcalTBWeights from DB " ;
#endif
   edm::ESHandle<EcalTBWeights> pWgts;
   es.get<EcalTBWeightsRcd>().get(pWgts);
   const EcalTBWeights* wgts = pWgts.product();
   const EcalTBWeights::EcalTBWeightMap& wgtsMap = wgts->getMap();
#ifdef DEBUG
   LogDebug("EcalUncalibRecHitDebug") << "EcalTBWeightMap.size(): " << std::setprecision(3) << wgtsMap.size() ;
#endif

   // fetch the pedestals from the cond DB via EventSetup
#ifdef DEBUG
   LogDebug("EcalUncalibRecHitDebug") << "fetching pedestals....";
#endif
   edm::ESHandle<EcalPedestals> pedHandle;
   es.get<EcalPedestalsRcd>().get( pedHandle );
   const EcalPedestalsMap& pedMap = pedHandle.product()->m_pedestals; // map of pedestals
#ifdef DEBUG
   LogDebug("EcalUncalibRecHitDebug") << "done." ;
#endif
   // collection of reco'ed ampltudes to put in the event

   std::auto_ptr< EBUncalibratedRecHitCollection > EBuncalibRechits( new EBUncalibratedRecHitCollection );
   std::auto_ptr< EEUncalibratedRecHitCollection > EEuncalibRechits( new EEUncalibratedRecHitCollection );

   EcalPedestalsMapIterator pedIter; // pedestal iterator

   EcalGainRatios::EcalGainRatioMap::const_iterator gainIter; // gain iterator

   EcalWeightXtalGroups::EcalXtalGroupsMap::const_iterator git; // group iterator

   EcalTBWeights::EcalTBWeightMap::const_iterator wit; //weights iterator 

   // loop over EB digis
   if (EBdigis)
     {
       for(EBDigiCollection::const_iterator itdg = EBdigis->begin(); itdg != EBdigis->end(); ++itdg) {

	 //     counter_++; // verbosity counter

	 // find pedestals for this channel
#ifdef DEBUG
	 LogDebug("EcalUncalibRecHitDebug") << "looking up pedestal for crystal: " << EBDetId(itdg->id()) ;
#endif
	 pedIter = pedMap.find(itdg->id().rawId());
	 if( pedIter == pedMap.end() ) {
	   edm::LogError("EcalUncalibRecHitError") << "error!! could not find pedestals for channel: " << EBDetId(itdg->id()) 
						   << "\n  no uncalib rechit will be made for this digi!"
	     ;
	   continue;
	 }
	 const EcalPedestals::Item& aped = pedIter->second;
	 double pedVec[3];
	 pedVec[0]=aped.mean_x12;pedVec[1]=aped.mean_x6;pedVec[2]=aped.mean_x1;

	 // find gain ratios
#ifdef DEBUG
	 LogDebug("EcalUncalibRecHitDebug") << "looking up gainRatios for crystal: " << EBDetId(itdg->id()) ;
#endif
	 gainIter = gainMap.find(itdg->id().rawId());
	 if( gainIter == gainMap.end() ) {
	   edm::LogError("EcalUncalibRecHitError") << "error!! could not find gain ratios for channel: " << EBDetId(itdg->id()) 
						   << "\n  no uncalib rechit will be made for this digi!"
	     ;
	   continue;
	 }
	 const EcalMGPAGainRatio& aGain = gainIter->second;
	 double gainRatios[3];
	 gainRatios[0]=1.;gainRatios[1]=aGain.gain12Over6();gainRatios[2]=aGain.gain6Over1()*aGain.gain12Over6();

	 // lookup group ID for this channel
	 git = grpMap.find( itdg->id().rawId() );
	 if( git == grpMap.end() ) {
	   edm::LogError("EcalUncalibRecHitError") << "No group id found for this crystal. something wrong with EcalWeightXtalGroups in your DB?"
						   << "\n  no uncalib rechit will be made for digi with id: " << EBDetId(itdg->id())
	     ;
	   continue;
	 }
	 const EcalXtalGroupId& gid = git->second;	 
	 // use a fake TDC iD for now until it become available in raw data
	 EcalTBWeights::EcalTDCId tdcid(1);
	 
	 // now lookup the correct weights in the map
	 wit = wgtsMap.find( std::make_pair(gid,tdcid) );
	 if( wit == wgtsMap.end() ) {  // no weights found for this group ID
	   edm::LogError("EcalUncalibRecHitError") << "No weights found for EcalGroupId: " << gid.id() << " and  EcalTDCId: " << tdcid
						   << "\n  skipping digi with id: " << EBDetId(itdg->id())
	     ;
	   continue;
	 }
	 const EcalWeightSet& wset = wit->second; // this is the EcalWeightSet

	 // EcalWeightMatrix is vec<vec:double>>
#ifdef DEBUG
	 LogDebug("EcalUncalibRecHitDebug") << "accessing matrices of weights...";
#endif
	 const math::EcalWeightMatrix::type& mat1 = wset.getWeightsBeforeGainSwitch();
	 const math::EcalWeightMatrix::type& mat2 = wset.getWeightsAfterGainSwitch();
	 const math::EcalChi2WeightMatrix::type& mat3 = wset.getChi2WeightsBeforeGainSwitch();
	 const math::EcalChi2WeightMatrix::type& mat4 = wset.getChi2WeightsAfterGainSwitch();
#ifdef DEBUG
	 LogDebug("EcalUncalibRecHitDebug") << "done." ;
#endif
	 // build CLHEP weight matrices
	 const math::EcalWeightMatrix::type* weights[2];
	 weights[0]=&mat1;
	 weights[1]=&mat2;
// 	 weights.push_back(clmat1);
// 	 weights.push_back(clmat2);
// 	 LogDebug("EcalUncalibRecHitDebug") << "weights before switch:\n" << clmat1 ;
// 	 LogDebug("EcalUncalibRecHitDebug") << "weights after switch:\n" << clmat2 ;


	 // build CLHEP chi2  matrices
	 const math::EcalChi2WeightMatrix::type* chi2mat[2];
	 chi2mat[0]=&mat3;
	 chi2mat[1]=&mat4;

	 //if(!counterExceeded()) LogDebug("EcalUncalibRecHitDebug") << "chi2 matrix before switch:\n" << clmat3 ;
	 //if(!counterExceeded()) LogDebug("EcalUncalibRecHitDebug") << "chi2 matrix after switch:\n" << clmat4 ;

	 EcalUncalibratedRecHit aHit =
	   EBalgo_.makeRecHit(*itdg, pedVec, gainRatios, weights, chi2mat);
	 EBuncalibRechits->push_back( aHit );

#ifdef DEBUG
	 if(aHit.amplitude()>0.) {
	   LogDebug("EcalUncalibRecHitDebug") << "processed EBDataFrame with id: "
					      << EBDetId(itdg->id()) << "\n"
					      << "uncalib rechit amplitude: " << aHit.amplitude()
	     ;
	 }
#endif
       }
     }

   // loop over EB digis
   if (EEdigis)
     {
       for(EEDigiCollection::const_iterator itdg = EEdigis->begin(); itdg != EEdigis->end(); ++itdg) {

	 //     counter_++; // verbosity counter

	 // find pedestals for this channel
#ifdef DEBUG
	 LogDebug("EcalUncalibRecHitDebug") << "looking up pedestal for crystal: " << EEDetId(itdg->id()) ;
#endif
	 pedIter = pedMap.find(itdg->id().rawId());
	 if( pedIter == pedMap.end() ) {
	   edm::LogError("EcalUncalibRecHitError") << "error!! could not find pedestals for channel: " << EEDetId(itdg->id()) 
						   << "\n  no uncalib rechit will be made for this digi!"
	     ;
	   continue;
	 }
	 const EcalPedestals::Item& aped = pedIter->second;
	 double pedVec[3];
	 pedVec[0]=aped.mean_x12;pedVec[1]=aped.mean_x6;pedVec[2]=aped.mean_x1;

	 // find gain ratios
#ifdef DEBUG
	 LogDebug("EcalUncalibRecHitDebug") << "looking up gainRatios for crystal: " << EEDetId(itdg->id()) ;
#endif
	 gainIter = gainMap.find(itdg->id().rawId());
	 if( gainIter == gainMap.end() ) {
	   edm::LogError("EcalUncalibRecHitError") << "error!! could not find gain ratios for channel: " << EEDetId(itdg->id()) 
						   << "\n  no uncalib rechit will be made for this digi!"
	     ;
	   continue;
	 }
	 const EcalMGPAGainRatio& aGain = gainIter->second;
	 double gainRatios[3];
	 gainRatios[0]=1.;gainRatios[1]=aGain.gain12Over6();gainRatios[2]=aGain.gain6Over1()*aGain.gain12Over6();

	 // lookup group ID for this channel
	 git = grpMap.find( itdg->id().rawId() );
	 if( git == grpMap.end() ) {
	   edm::LogError("EcalUncalibRecHitError") << "No group id found for this crystal. something wrong with EcalWeightXtalGroups in your DB?"
						   << "\n  no uncalib rechit will be made for digi with id: " << EEDetId(itdg->id())
	     ;
	   continue;
	 }
	 const EcalXtalGroupId& gid = git->second;	 
	 // use a fake TDC iD for now until it become available in raw data
	 EcalTBWeights::EcalTDCId tdcid(1);
	 
	 // now lookup the correct weights in the map
	 wit = wgtsMap.find( std::make_pair(gid,tdcid) );
	 if( wit == wgtsMap.end() ) {  // no weights found for this group ID
	   edm::LogError("EcalUncalibRecHitError") << "No weights found for EcalGroupId: " << gid.id() << " and  EcalTDCId: " << tdcid
						   << "\n  skipping digi with id: " << EEDetId(itdg->id())
	     ;
	   continue;
	 }
	 const EcalWeightSet& wset = wit->second; // this is the EcalWeightSet

	 // EcalWeightMatrix is vec<vec:double>>
#ifdef DEBUG
	 LogDebug("EcalUncalibRecHitDebug") << "accessing matrices of weights...";
#endif
	 const math::EcalWeightMatrix::type& mat1 = wset.getWeightsBeforeGainSwitch();
	 const math::EcalWeightMatrix::type& mat2 = wset.getWeightsAfterGainSwitch();
	 const math::EcalChi2WeightMatrix::type& mat3 = wset.getChi2WeightsBeforeGainSwitch();
	 const math::EcalChi2WeightMatrix::type& mat4 = wset.getChi2WeightsAfterGainSwitch();
#ifdef DEBUG
	 LogDebug("EcalUncalibRecHitDebug") << "done." ;
#endif
	 // build CLHEP weight matrices
	 const math::EcalWeightMatrix::type* weights[2];
	 weights[0]=&mat1;
	 weights[1]=&mat2;
// 	 weights.push_back(clmat1);
// 	 weights.push_back(clmat2);
// 	 LogDebug("EcalUncalibRecHitDebug") << "weights before switch:\n" << clmat1 ;
// 	 LogDebug("EcalUncalibRecHitDebug") << "weights after switch:\n" << clmat2 ;


	 // build CLHEP chi2  matrices
	 const math::EcalChi2WeightMatrix::type* chi2mat[2];
	 chi2mat[0]=&mat3;
	 chi2mat[1]=&mat4;

	 //if(!counterExceeded()) LogDebug("EcalUncalibRecHitDebug") << "chi2 matrix before switch:\n" << clmat3 ;
	 //if(!counterExceeded()) LogDebug("EcalUncalibRecHitDebug") << "chi2 matrix after switch:\n" << clmat4 ;

	 EcalUncalibratedRecHit aHit =
	   EEalgo_.makeRecHit(*itdg, pedVec, gainRatios, weights, chi2mat);
	 EEuncalibRechits->push_back( aHit );

#ifdef DEBUG
	 if(aHit.amplitude()>0.) {
	   LogDebug("EcalUncalibRecHitDebug") << "processed EEDataFrame with id: "
					      << EEDetId(itdg->id()) << "\n"
					      << "uncalib rechit amplitude: " << aHit.amplitude()
	     ;
	 }
#endif
       }
     }

   // put the collection of recunstructed hits in the event
   evt.put( EBuncalibRechits, EBhitCollection_ );
   evt.put( EEuncalibRechits, EEhitCollection_ );
//    evt.put( EEuncalibRechits, EEhitCollection_ );
}



