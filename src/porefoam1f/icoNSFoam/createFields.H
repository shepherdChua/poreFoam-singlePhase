
    Info<< "Reading field p\n" << endl;
    volScalarField p
    (  IOobject
        (  "p",  runTime.timeName(),  mesh,
            IOobject::MUST_READ,  IOobject::AUTO_WRITE
        ),  mesh
    );


	Info<< "Reading field U\n" << endl;
	volVectorField U (
		IOobject( "U",  runTime.timeName(),  mesh,
			       IOobject::MUST_READ,  IOobject::AUTO_WRITE ),
		mesh );

	#include "createPhi.H"


    U.correctBoundaryConditions();

	volScalarField pOldOld=p; 
	volScalarField pOld=p; 


    Info<< "Reading transportProperties\n" << endl;
    IOdictionary transportProperties
    (	IOobject
		( "transportProperties",  runTime.constant(),  runTime,
		IOobject::MUST_READ,  IOobject::NO_WRITE
    )	);
    
	const dimensionedScalar rho(transportProperties.lookup("rho"));
	const dimensionedScalar nu(transportProperties.lookup("nu"));
	const dimensionedScalar mu(rho*nu);
	



	scalar cBC =readScalar( pimple.dict().lookup("cBC")  );



	label pRefCell = 0;
	scalar pRefValue = 0.0;
	setRefCell(p, pimple.dict(), pRefCell, pRefValue);




	scalar thresholdDelUPer10Step( pimple.dict().lookupOrDefault("thresholdDelUPer10Step", 0.0005) );
	scalar oldelAvgUPer10Step = 1.0e+64;
	scalar oldAvgU = 0.0;



	scalar lambda( readScalar(pimple.dict().lookup("lambda")) );


	scalar deltaTFact=1.0;

	label nUCorr( readLabel(pimple.dict().lookup("nUCorr")) );

	dimensionedScalar beta ( pimple.dict().lookup("beta")  );



	Info<<"average(fvc::laplacian(p)): "<<average(fvc::laplacian(p)).value()<<endl; //just to make sure Everything is computed before cleaning up below
	Info<<"average(fvc::div(U)): "<<average(fvc::div(U)).value()<<endl; //just to make sure Everything is computed before cleaning up below
	mesh.clearPoints();
	mesh.clearNonOrtho();


