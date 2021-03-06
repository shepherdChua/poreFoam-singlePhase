/*-------------------------------------------------------------------------*\
This code is part of poreFOAM, a suite of codes written using OpenFOAM
for direct simulation of flow at the pore scale. 	
You can redistribute this code and/or modify this code under the 
terms of the GNU General Public License (GPL) as published by the  
Free Software Foundation, either version 3 of the License, or (at 
your option) any later version. see <http://www.gnu.org/licenses/>.



Developed by Ali Qaseminejad Raeini
* 
Please see our website for relavant literature:
http://www3.imperial.ac.uk/earthscienceandengineering/research/perm/porescalemodelling

For further information please contact us by email:
Ali Q Raeini:    a.qaseminejad-raeini09@imperial.ac.uk

\*-------------------------------------------------------------------------*/

#define SINGLE_PHASE
#define ifMonitor  if( runTime.timeIndex()%10== 0 ) 

#include "fvCFD.H"


//#include "singlePhaseTransportModel.H"
//#include "turbulentTransportModel.H"


#include "fixedFluxPressureFvPatchScalarField.H"

#include "pimpleControl.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //



int main(int argc, char *argv[])
{
	#include "setRootCase.H"
	#include "createTime.H"
	#include "createMesh.H"
	if (!mesh.cells().size()) {Info<<"Error: no cells in (processor) mesh"<<endl; exit(-1);}
	pimpleControl pimple(mesh);
	#include "initContinuityErrs.H"
	#include "createFields.H"
	#include "createTimeControls.H"
	#include "correctPhi.H"
	#include "CourantNo.H"

	//#include "setInitialDeltaT.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

	Info<< "\nStarting time loop\n" << endl;
#define 	curtail(a,b) (min (max((a),(b)),(1.-(b))))

	ifMonitor
	{
		Info<< "\n         Umax = " << max(mag(U)).value() << " m/s  "
		<< "Uavg = " << mag(average(U)).value() << " m/s"
		<< "   DP = " << (max(p)-min(p)).value() << " Pa"
		<< nl<< nl << endl;
	}

	Info <<"min(p): "<<min(p)<<"  max(p): "<<max(p)<<endl;



	scalar pRelaxF=0.1;




	while (runTime.run())
	{
		scalar timeStepExecutionTime= runTime.elapsedCpuTime() ;
		//#include "readPIMPLEControls.H"
		#include "readTimeControls.H"
		#include "CourantNo.H"
		#include "setDeltaT.H"

		runTime++;

		Info<< "Time = " << runTime.timeName() << nl << endl;



		dimensionedScalar sgPc("sgPc", dimPressure/dimLength*dimArea, 0.0);
		while (pimple.loop())
		{
			//rhoPhi = rho1*phi;


			{
				volScalarField pOldCopy=p; 
				p=0.30*pOldOld+0.34*pOld+0.36*p;
				pOldOld=pOld;
				pOld=pOldCopy;
			}

			scalar tOldPU= runTime.elapsedCpuTime() ;



			#include "UEqn.H"

			ifMonitor   { Info<< "ExeTime - tOldPU = " << runTime.elapsedCpuTime()-tOldPU << " s"	<< endl;}

			while (pimple.correct())// --- PISO loop
			{
				#include "pEqn.H"
			}

		}

		#include "continuityErrs.H"






		Info<< "ExecutionTime = " << runTime.elapsedCpuTime() << " s"
			<< "  ClockTime = " << runTime.elapsedClockTime() << " s"
			<< "  timeStepExecutionTime  = " << runTime.elapsedCpuTime()-timeStepExecutionTime << " s"
			<< endl;

		ifMonitor
		{
			scalar maxU = max(mag(U)).value();
			scalar avgU = average(mag(U)).value();

			Info<< "\n         maxMagU = " <<maxU << " m/s  "
			<< "avgMagU = " << avgU << " m/s"
			<< "   DP = " << (max(p)-min(p)).value() << " Pa"
			<< nl<< nl << endl;
			
			scalar delAvgUPer10Step =  mag(avgU - oldAvgU);
			if (delAvgUPer10Step<thresholdDelUPer10Step*max(avgU,oldAvgU) && oldelAvgUPer10Step<thresholdDelUPer10Step*max(avgU,oldAvgU))
			{
				Info<< "converged ! " 	<< endl;
				
				runTime.writeAndEnd();
			}
			Info<<"! convergence: "<<delAvgUPer10Step<<"<"<<thresholdDelUPer10Step*max(avgU,oldAvgU) <<" && "<< oldelAvgUPer10Step<<"<"<<thresholdDelUPer10Step*max(avgU,oldAvgU)<<endl;
			oldelAvgUPer10Step=delAvgUPer10Step;
			oldAvgU=avgU;
		}




		scalar maxSimTime( readScalar(runTime.controlDict().lookup("maxExecutionTime")) );

		if (runTime.elapsedClockTime()>maxSimTime)
		{
			Info<< "ExecutionTime = " << runTime.elapsedCpuTime() << " s"
				<< "  ClockTime = " << runTime.elapsedClockTime() << " s\n"
				<< maxSimTime/24/60/60<<"  days passed!, ending simulation. " 
				<< endl;

			runTime.writeAndEnd();
		}

		#include "write.H"
	}

	if(!std::ifstream(runTime.timeName()+"/p").good())
	{
		Info<<"Error Pressure is not written, trying again:";
		U.write();
		phi.write();
		p.write();
	}

	mesh.clearPoints();// just to print the message
	mesh.clearNonOrtho();// just to print the message

    Info<< "End\n" << endl;

    return 0;
}


//----------------------------------------------------------------------
