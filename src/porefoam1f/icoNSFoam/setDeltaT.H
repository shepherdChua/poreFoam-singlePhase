/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 1991-2009 OpenCFD Ltd.
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

Global
    setDeltaT

Description
    Reset the timestep to maintain a constant maximum courant Number.
    Reduction of time-step is immediate, but increase is damped to avoid
    unstable oscillations.

\*---------------------------------------------------------------------------*/

if (adjustTimeStep)
{
	if(!(CoNum<10000.0)){Info<<"Error CoNum="<<CoNum<<endl;  exit(-1);}

    deltaTFact =  min(maxCo/(CoNum + SMALL),1.2);



    if(( (deltaTFact-1.0)>0.05 )||( (1.0-deltaTFact)>0.02 ))
    {
        runTime.setDeltaT
        (
            min
            (
                deltaTFact*runTime.deltaT().value(),
                maxDeltaT
            )
        );
    }
    
    Info<< "deltaT = " <<  runTime.deltaT().value() << endl;
}

// ************************************************************************* //
