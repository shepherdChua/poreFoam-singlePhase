/*-------------------------------------------------------------------------*\
This code is part of poreFOAM, a suite of codes written using OpenFOAM
for direct simulation of flow at the pore scale. 	
You can redistribute this code and/or modify this code under the 
terms of the GNU General Public License (GPL) as published by the  
Free Software Foundation, either version 3 of the License, or (at 
your option) any later version. see <http://www.gnu.org/licenses/>.


Please see our website for relavant publications:
https://www.imperial.ac.uk/engineering/departments/earth-science/research/research-groups/perm/research/pore-scale-modelling/

For further information please contact me by email:
Ali Qaseminejad Raeini:    a.qaseminejad-raeini09@imperial.ac.uk
\*-------------------------------------------------------------------------*/

#include <fstream>
#include <assert.h>

#include "fvCFD.H"

#include "argList.H"
#include "timeSelector.H"
#include "graph.H"
#include "mathematicalConstants.H"
//#include "incompressibleTwoPhaseMixture.H"
#include "pimpleControl.H"

#include "myFVC.H"
#include <valarray>
#include "voxelImage.h"
#include "AverageData.h"

using namespace Foam;

std::ostream & operator << (std::ostream & outstream, const std::valarray<std::valarray<double> >& vecvec)
{
	for (size_t i=0; i<vecvec[0].size();++i)
	{
		for (size_t j=0; j<vecvec.size();++j) outstream << vecvec[j][i] << ' ';
		outstream << '\n';
	}
	outstream << '\n';
	return outstream;
}

std::valarray<std::valarray<double> > distribution(const scalarField & UcompNormed, const scalarField & Vol, double minmax=6)
{


	std::valarray<std::valarray<double> > distrib(std::valarray<double>(0.0, 128),3);


	double minU=gMin(UcompNormed);
	double maxU=max(gMax(UcompNormed),minmax);
	double spanU=(maxU-minU);
	double deltaU=spanU/128.0+1.0e-72;

	for (int i=0; i<128; ++i)	distrib[0][i] = minU+deltaU/2+i*deltaU;

	for (int i=0; i<Vol.size(); ++i)
	{
		int distInd=min(int((UcompNormed[i]-minU)/deltaU+0.5),127);
		++distrib[1][distInd];
		distrib[2][distInd]+=Vol[i];
	}

	for (int i=0; i<128; ++i)
	{
		scalar disti=distrib[1][i];
		reduce(disti, sumOp<scalar>());
		distrib[1][i] = disti;
		scalar distiv=distrib[2][i];
		reduce(distiv, sumOp<scalar>());
		distrib[2][i] = distiv;
	}

	distrib[1]/=distrib[1].sum()*deltaU;
	distrib[2]/=distrib[2].sum()*deltaU;


	return distrib;
}


//----------------------------------------------------------------------
// Main program:

int main(int argc, char *argv[])
{
	#include "setRootCase.H"
	#include "createTime.H"
	instantList timeDirs = timeSelector::select0(runTime, args);


	#include "createNamedMesh.H"
	pimpleControl pimple(mesh);



	//Info<< "Reading transportProperties\n" << endl;
	//IOdictionary transportProperties
	//(  IOobject
		//(  "transportProperties",  runTime.constant(),  runTime,
			//IOobject::MUST_READ,  IOobject::NO_WRITE
	//)	);

	runTime.setTime(timeDirs[timeDirs.size()-1], timeDirs.size()-1);


	#define x_ 0 
	#define y_ 1 
	#define z_ 2 

	scalar L[3];
	scalar K[3];
	vector dp;
	vector VDarcy;
	scalar dx=pow(average(mesh.V()), 1.0/3.0).value();
	L[x_]=(gMax(mesh.points().component(0))-gMin(mesh.points().component(0)));
	L[y_]=(gMax(mesh.points().component(1))-gMin(mesh.points().component(1)));
	L[z_]=(gMax(mesh.points().component(2))-gMin(mesh.points().component(2)));
	Info<<" "<< dx <<" "<< L[x_] <<" "<< L[y_] <<" "<< L[z_] <<" "<< endl;

	scalar A[]={L[y_]*L[z_],L[z_]*L[x_],L[x_]*L[y_]};
	word LeftPs[]={"Left","Bottom","Front"};//
	word RightPs[]={"Right","Top","Back"};//
	word directions[]={"x","y","z"};//


	#include "./createFields.H"



  #include "createCVs.H"

	const volVectorField& C=mesh.C();

	int nSams = regionVoxelValues.size();
	for (int iSam=0;iSam<nSams;iSam++)   
	{
		clip=0.0;
		forAll(C,c) 
		{ 
			if (PPRegions[c]==regionVoxelValues[iSam]&& 
				(C[c][0]>=CVBounds1[iSam] && C[c][0]<=CVBounds2[iSam] && 
				 C[c][1]>=CVyBounds1[iSam] && C[c][1]<=CVyBounds2[iSam] && 
				 C[c][2]>=CVzBounds1[iSam] && C[c][2]<=CVzBounds2[iSam] ))
				clip[c]=1.0;
		}

		word weightporo = transportProperties.lookupOrDefault("weight",word("porosity"));
		volScalarField porosity
		(	IOobject
			(	weightporo,	"0",	mesh, IOobject::READ_IF_PRESENT
			),	max(clip,1.0e-64)
		);
		porosity=max(porosity,1.0e-17);


		if(max(porosity).value()<0.99999)
			Info<<"micro-porosity: ["<<min(porosity).value()<<", "<<max(porosity).value()<<"], avg: "<<average(porosity).value()<<endl;

		label iDir;
		for (iDir=0;iDir<3;iDir++)
		{
			label iBegin = mesh.boundaryMesh().findPatchID(LeftPs[iDir]);
			if (iBegin < 0)	 { Info	<< "Unable to find  patch " << LeftPs[iDir] << nl	<< endl;	 continue; }
			label iEnd = mesh.boundaryMesh().findPatchID(RightPs[iDir]);
			if (iEnd < 0) { Info	<< "Unable to find  patch " << RightPs[iDir] << nl	<< endl;	 continue; }


			scalar fluxIn=gSum(phi.boundaryField()[iBegin]+1.0e-63);
			scalar fluxOut=gSum(phi.boundaryField()[iEnd]+1.0e-63);


			scalar PLeft=gSum(p.boundaryField()[iBegin]*(phi.boundaryField()[iBegin]+1.0e-63))/(fluxIn+1.0e-163);
			scalar PRight=gSum(p.boundaryField()[iEnd]*(phi.boundaryField()[iEnd]+1.0e-63))/(fluxOut+1.0e-163);
			dp[iDir]=mag(PLeft-PRight);

			fluxIn=gSum(phi.boundaryField()[iBegin]);

			K[iDir]=mag(fluxIn)*mu.value()/A[iDir]*L[iDir]/(dp[iDir]+1.0e-64);
			VDarcy[iDir]=mag(fluxIn/A[iDir]);
		}

		iDir=findMax(dp);
		scalar Pmax_min=(max(p)-min(p)).value();
		scalar Umax=(max(mag(U))).value();
		scalar Re=rho.value()*VDarcy[iDir]*std::sqrt(K[iDir])/mu.value() ;
		double porVol = gSum(mesh.V()*porosity.internalField());

		Info << runTime.caseName()
			<<"\t\t effPorosity=  "<<porVol/(L[x_]*L[y_]*L[z_])<<"                  = V_pore/(L_x*L_y*L_z)= "<<porVol<<" /( "<<L[x_]<<" "<<L[y_]<<" "<<L[z_]<<" )\n"
			<<"\tK_"<<directions[iDir]<<"= "<<  K[iDir]<<" m^2 \t"
			<<"\tDarcyVelocity= "<< VDarcy[iDir] <<" m/s \t"
			<<"\tDelP= "<< dp[iDir] <<" Pa \t"
			<<"\tK: "<<  K[iDir]<<" m^2 \t"
			<<"\t\tK=( "<<  K[x_]<<"  "<<  K[y_]<<"  "<<  K[z_] <<" )"
			<<" Pmax-Pmin: "<<  Pmax_min
			<<" Umax= "<<  Umax
			<<"\t\t Re= "<<  "rho*VDarcy*sqrt(K)/mu= "<< rho.value() <<"*"<<VDarcy[iDir]<<"*sqrt("<<K[iDir]<<")/"<<mu.value()<<")= "<<Re 
			<< "\n";
		VDarcy[iDir]=max(1.0e-64,VDarcy[iDir]);
		std::valarray<std::valarray<double> > ditribLogU = distribution(log10(max(1.0e-16,clip.internalField()*mag(U.internalField())/porosity.internalField()/VDarcy[iDir]) ), mesh.V()*porosity.internalField());
		std::valarray<std::valarray<double> > ditribU  = distribution(clip.internalField()*mag(U.internalField())/porosity.internalField()/VDarcy[iDir], mesh.V());
		std::valarray<std::valarray<double> > ditribUx = distribution(clip.internalField()*U.internalField().component(vector::X)/porosity.internalField()/VDarcy[iDir], mesh.V()*porosity.internalField());
		std::valarray<std::valarray<double> > ditribUy = distribution(clip.internalField()*U.internalField().component(vector::Y)/porosity.internalField()/VDarcy[iDir], mesh.V()*porosity.internalField());
		std::valarray<std::valarray<double> > ditribUz = distribution(clip.internalField()*U.internalField().component(vector::Z)/porosity.internalField()/VDarcy[iDir], mesh.V()*porosity.internalField());

		std::valarray<std::valarray<double> > ditribUmCbrt = distribution(cbrt(clip.internalField()*mag(U.internalField())/porosity.internalField()/VDarcy[iDir]), mesh.V()*porosity.internalField(),-1.0e9);
		std::valarray<std::valarray<double> > ditribUxCbrt = distribution(cbrt(clip.internalField()*U.internalField().component(0)/porosity.internalField()/VDarcy[iDir]), mesh.V()*porosity.internalField(),-1.0e9);
		std::valarray<std::valarray<double> > ditribUyCbrt = distribution(cbrt(clip.internalField()*U.internalField().component(1)/porosity.internalField()/VDarcy[iDir]), mesh.V()*porosity.internalField(),-1.0e9);
		std::valarray<std::valarray<double> > ditribUzCbrt = distribution(cbrt(clip.internalField()*U.internalField().component(2)/porosity.internalField()/VDarcy[iDir]), mesh.V()*porosity.internalField(),-1.0e9);


		std::valarray<std::valarray<double> > ditribLogUPlus(ditribLogU[0],5);
		ditribLogUPlus[0]=std::pow(10.0,ditribLogU[0]);
		ditribLogUPlus[1]=ditribLogU[1]/ditribLogUPlus[0]/Foam::log(10.0);
		ditribLogUPlus[2]=ditribLogU[2]/ditribLogUPlus[0]/Foam::log(10.0);
		ditribLogUPlus[3]=ditribLogU[1];
		ditribLogUPlus[4]=ditribLogU[2];


		vector FF(0.0, 0.0, 0.0);
		if(nSams==1)
		{
			#include "calc_FF.H"
		}
		else Info<<"\n\nInfo: Skipping calc_FF in multi_region upscaling mdoe\n\n"<<endl;

		Info	<< runTime.caseName()
				<<"\n\neffPorosity=   "<<porVol/(L[x_]*L[y_]*L[z_])<<"                  = V_pore/(L_x*L_y*L_z)= "<<porVol<<" /( "<<L[x_]<<" "<<L[y_]<<" "<<L[z_]<<" )\n"
				<<"K_"<<directions[iDir]<<"=           "<<  K[iDir]<<" m^2,        \t  K=( "<<K[x_]<<"  "<<K[y_]<<"  "<<K[z_]<<" ) \n";
		if(nSams==1) Info <<"FF_"<<directions[iDir]<<"=          "<<  FF[iDir]<<" ";
		Info	<<"\nDarcyVelocity= "<< VDarcy[iDir] <<" m/s,    \t   Umax= "<<  Umax <<"\t  DelP= "<< dp[iDir] <<" Pa "<<",  Pmax-Pmin: "<<  Pmax_min <<" \n"
				<<"Re=            "<<Re<<  "             =rho*VDarcy*sqrt(K)/mu= "<< rho.value() <<" "<<VDarcy[iDir]<<" sqrt( "<<K[iDir]<<" )/ "<<mu.value()<<" )" 
				<< "\n\n";



		if (Pstream::master())
		{
			std::string title=runTime.caseName();

			size_t slashloc=title.find_last_of("\\/"); if (slashloc<title.size()) title.erase(slashloc, std::string::npos);
			slashloc=title.find_last_of("\\/"); if (slashloc<title.size()) title=title.substr(slashloc+1);
			Info <<"title: " <<title<<endl;
			if(nSams>1)  title=title+toStr(iSam);
			std::ofstream of("summary_"+title+".txt"/*,std::ios::app*/);
			assert(of);

			of  << runTime.caseName()<<"  t="<<runTime.value()
				<<"\n\neffPorosity=   "<< porVol/(L[x_]*L[y_]*L[z_]) <<"                  = V_pore/(L_x*L_y*L_z)= "<<porVol<<" /( "<<L[x_]<<" "<<L[y_]<<" "<<L[z_]<<" )\n"
				<<"K_"<<directions[iDir]<<"=           "<<  K[iDir]<<" m^2,          K=( "<<  K[x_]<<"  "<<  K[y_]<<"  "<<  K[z_] <<" ) \n";
			if(nSams==1) of	<<"FF_"<<directions[iDir]<<"=          "<<  FF[iDir]<<" ";
			of	<<"\nDarcyVelocity= "<< VDarcy[iDir] <<" m/s,    \t   Umax= "<<  Umax <<"\t  DelP= "<< dp[iDir] <<" Pa "<<",  Pmax-Pmin: "<<Pmax_min<<" \n"
				<<"Re=            "<<Re<<  "             =  rho*VDarcy*sqrt(K)/mu= "<< rho.value()<<"*"<<VDarcy[iDir]<<"*sqrt("<<K[iDir]<<")/"<<mu.value()<<")" 
				<< "\n\n";

			of<<"\n\nx=mag(U)/U_D \t PDF \t dV/Vdx \t PDF(log10(x)) \t dV/Vd(log10(x))"<<std::endl;
			of<<ditribLogUPlus<<std::endl;
			of<<"\n\nx=U_x/U_D \t PDF \t dV/Vdx"<<std::endl;
			of<<ditribUx<<std::endl;
			of<<"\n\nx=U_y/U_D \t PDF \t dV/Vdx"<<std::endl;
			of<<ditribUy<<std::endl;
			of<<"\n\nx=U_z/U_D \t PDF \t dV/Vdx"<<std::endl;
			of<<ditribUz<<std::endl;
			of<<"\n\nx=mag(U)/U_D \t PDF \t dV/Vdx"<<std::endl;
			of<<ditribU<<std::endl;


			of<<"\n\n distributions with uniform cbrt(U) interval "<<std::endl;
			of<<"\n\nx=U_m/U_D dummy \t PDF \t dV/Vdx \t distConstDelCbrtU:"<<std::endl;
			ditribUmCbrt[1]=3.0*ditribUmCbrt[0]*ditribUmCbrt[0]*ditribUmCbrt[1];
			ditribUmCbrt[2]=3.0*ditribUmCbrt[0]*ditribUmCbrt[0]*ditribUmCbrt[2];
			ditribUmCbrt[0]=ditribUmCbrt[0]*ditribUmCbrt[0]*ditribUmCbrt[0];
			of<<ditribUmCbrt<<std::endl;

			of<<"\n\nx=U_x/U_D dummy \t PDF \t dV/Vdx"<<std::endl;
			ditribUxCbrt[1]=3.0*ditribUxCbrt[0]*ditribUxCbrt[0]*ditribUxCbrt[1];
			ditribUxCbrt[2]=3.0*ditribUxCbrt[0]*ditribUxCbrt[0]*ditribUxCbrt[2];
			ditribUxCbrt[0]=ditribUxCbrt[0]*ditribUxCbrt[0]*ditribUxCbrt[0];
			of<<ditribUxCbrt<<std::endl;

			of<<"\n\nx=U_y/U_D dummy \t PDF \t dV/Vdx"<<std::endl;
			ditribUyCbrt[1]=3.0*ditribUyCbrt[0]*ditribUyCbrt[0]*ditribUyCbrt[1];
			ditribUyCbrt[2]=3.0*ditribUyCbrt[0]*ditribUyCbrt[0]*ditribUyCbrt[2];
			ditribUyCbrt[0]=ditribUyCbrt[0]*ditribUyCbrt[0]*ditribUyCbrt[0];
			of<<ditribUyCbrt<<std::endl;

			of<<"\n\nx=U_z/U_D dummy \t PDF \t dV/Vdx"<<std::endl;
			ditribUzCbrt[1]=3.0*ditribUzCbrt[0]*ditribUzCbrt[0]*ditribUzCbrt[1];
			ditribUzCbrt[2]=3.0*ditribUzCbrt[0]*ditribUzCbrt[0]*ditribUzCbrt[2];
			ditribUzCbrt[0]=ditribUzCbrt[0]*ditribUzCbrt[0]*ditribUzCbrt[0];
			of<<ditribUzCbrt<<std::endl;
			
			of.close();
		}
	}

	#include "calc_grads.H"
	Info<< "end" << endl;

	return 0;
}


//----------------------------------------------------------------------
