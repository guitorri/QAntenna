/***************************************************************************
 *   Copyright (C) 2005 by                                                 *
 *   Gustavo González - gonzalgustavo en/at gmail.com                      *
 *   Lisandro Damián Nicanor Pérez Meyer - perezmeyer en/at gmail.com      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "necinput.h"
#include "primitive.h"
#include "cards/allcards.h"
#include <math.h>
#include <QFile>
#include <QTextStream>
#include <QString>
#include <QtDebug>
#include <QDir>

const double PI = 3.14159265;

NECInput::NECInput(QString theFileName, QString theCreationTime,
                   QWidget * parent) : QObject(parent)
{
  fileName = theFileName;
  creationTime = theCreationTime;

  maxModule = -1.0;

  groundPlane = false;
  radius = 2;

  foundFRCard = false;

  // We create three values, which are Cartesian co-ordinates of the origin
  // of the space containing the antenna stuctures.
  centerPosition.append(0.0);
  centerPosition.append(0.0);
  centerPosition.append(0.0);
}

NECInput::~NECInput()
{
  GenericCard * theCard;
  for(int i=0; i<cardsList.size(); i++)
  {
    theCard = cardsList.takeFirst();
    delete theCard;
  }
}

void NECInput::appendCard(GenericCard * theCard)
{
  cardsList.append(theCard);
}

void NECInput::setMaxModule(double newMaxModule)
{
  maxModule = newMaxModule;
}

void NECInput::setSimulationFrequency(double newFrequency)
{
  frequency = newFrequency;
}

void NECInput::setRadius(double newRadius)
{
  radius = newRadius;
  render();
}

void NECInput::setFrequency(int type, int /*nSteps*/,
                            double newFrequency, double /*stepInc*/)
{
  /**
    Please note that at the time of writing QAntenna 0.2, we can only render
    the radiation pattern of just one frequency, so we will force nSteps = 1
    and stepInc = 0.
    If this changes at sometime, remember to check frcard.cpp for a change of
    the same type.
  */

  FRCard * frcard = 0;

  /*
    We must check the list for a FR card.
    * If the card exists, we must change it's values
    * If the card does not exists, we must add one in a the correct place
  */

  // Let's check is the FR card exists
  int i = 0;
  while(i<cardsList.size() and cardsList.at(i)->getCardType() != "FR")
    i++;

  // We check wheter the card has been found or not
  if(i<cardsList.size())
  {
    // We found the card, so we change the parameters
    frcard = (FRCard*)cardsList.at(i);
    frcard->setTypeOfStepping(type);
    frcard->setNumberOfFrequencySteps(1);
    frcard->setFrequency(newFrequency);
    frcard->setFrequencyIncrement(1);
    frcard = 0;
  }
  else
  {
    // There is no FR card, we create one
    frcard = new FRCard(type, 1, newFrequency, 0);
    // And we search for the first GE card
    i = 0;
    while(i<cardsList.size() and cardsList.at(i)->getCardType() != "GE")
      i++;
    if(i<cardsList.size())
    {
      // We found the GE card
      cardsList.insert(i+1,frcard);
      frcard = 0;
    }
    else
    {
      /*
        We could not find a GE card. Do we have any more options instead
        of quiting?
      */
      qDebug() << "NECInput::setFrequency";
      qDebug() << QWidget::tr("While trying to insert a FR card, I could not "
                              "find a GE card in order to put the first card "
                              "in a right place");
      return;
    }
  }
  // Now we must re-create input.necin
  createNECInputFile();
}

double NECInput::getMaxModule()
{
  return maxModule;
}

double NECInput::getRadius()
{
  return radius;
}

void NECInput::getPosition(double & newX, double & newY, double & newZ)
{
  newX = centerPosition.at(0)/maxModule;
  newY = centerPosition.at(1)/maxModule;
  newZ = centerPosition.at(2)/maxModule;
}

double NECInput::getFrequency() const
{
  return frequency;
}

void NECInput::processGMCard(int index)
{
  QVector<double> ang;
  QVector<double> pos;
  // Declare these to be integers.
  int its; 	// TagNumber of the transfomation begin.
  int nrpt;	// Number of new structures to be created.
  int itgg;	// Tag increment (Originally known as ITGI)

/*
 * ***************************************************************************
 * For clarity here, we explain GetEnd1() and GetEnd2() in relation to an
 * (ang)le quantity and a pos(itional) quantity.
 *
 * The concept of an "End1" and an "End2" relates to the ENDS of a wire element
 * defined by the six fields (4,5,6,7,8,9) in a "GW" card, these being Cartesian
 * co-ordinates representing Wire_End1(X,Y,Z) and Wire_End2(X,Y,Z).
 *
 * The functions GetEnd1() and GetEnd2() were named after their role to extract
 * these co-ordinates.
 *
 * In another type of card "GM", these very same field positions are used for
 * the entirely different quantities of angles and displacements as needed
 * to implement the "Coordinate Transformations" purposes of a "GM" card.
 *
 * When used in the context of "GM" card values, the GetEnd1() function is
 * actually fetching three *angles*, and GetEnd2() is actually fetching three
 * *translation displacements*. They fetch the correct numbers from the card
 * fields, even though they remain (pehaps inappropriately) named after the
 * quantities they were originally conceived to represent.
 *
 * ***************************************************************************
 */
  // Thus - for the GM Coordinate Transformations card.
  ang = primitiveList.at(index)->GetEnd1();	// Fetch 3 rotation angles.
  // ROX (F1), ROY (F2), ROZ (F3)

  pos = primitiveList.at(index)->GetEnd2();	// Fetch 3 translation displacements
  // XS (F4), YS (F5), ZS (F6)

  itgg = primitiveList.at(index)->GetTagNumber();// This might actually be ITGI
  // Original NEC variable was ITGI - the Tag Number Increment in field (I1)

  its = primitiveList.at(index)->GetCardParameter1();// OCR error in documentation
  // Unsure how to use it. If ITS is blanki (usual case) or zero, then the
  // entire structure is moved. This one is in (F7)

  nrpt = primitiveList.at(index)->GetCardParameter();// Number of repeats (I2)
  // If NRPT is zero, the structure is moved by the specified rotation and
  // translation, leaving nothing in the original location. If NRPT > 0,
  // original structures remain, and repeated new structures are formed, each
  // shifted from the previous by the requested transformation.

  primitiveList.removeAt(index);

  if(nrpt==0)
  { // GM with NRPT = 0.
    for(int i=0; i<index; i++)
    {
      // Check if tagNumber starts always at 1.
      if(primitiveList.at(i)->GetTagNumber()>=its)
      {
        primitiveList.at(i)->Rotate(ang);
        primitiveList.at(i)->Move(pos);
        compareModule(primitiveList.at(i)->CalculateMaxModule());

        /*
          We save the antenna's drawing displacement centre, wich we will use to
          displace the RP in the same ammount.
          ---------------------------
          After much testing, it is clear that the RP cannot be shifted in
          this way without introducing major errors, particularly when
          grounds are specified. I (think) centerPosition[] is only ever
          used to displace RP. The drawn structure seems to come out OK
          Therefore - pending any new information about serious departures
          of benchmark results compared to other nec2 programs, I leave
          the offset set to zero - Graham
         */
        // centerPosition[0] = pos[0];
        // centerPosition[1] = pos[1];
        // centerPosition[2] = pos[2];
        centerPosition[0] = 0.0;
        centerPosition[1] = 0.0;
        centerPosition[2] = 0.0;
      }
    }
  }else
  {
    // GM with NRPT > 0. We must generate new structures.
    Line* newLine;
    Patch* newPatch;
    Primitive* oldPrim;
    QVector<double> end1;
    QVector<double> end2;
    QVector<double> end3;
    QVector<double> end4;

    for(int i=0; i<nrpt; i++)
    {
      for(int j=0+i*index; j<(i+1)*index; j++)
      {
        oldPrim = primitiveList.at(j);
        if(oldPrim->GetLabel()=="GW")		// It's a line.
        {
          end1 = oldPrim->GetEnd1();
          end2 = oldPrim->GetEnd2();

          newLine = new Line("GW", end1, end2, oldPrim->GetTagNumber()+itgg, 0, 0);
          newLine->Rotate(ang);
          newLine->Move(pos);
          compareModule(newLine->CalculateMaxModule());
          primitiveList.insert((j+index),newLine);
        }
        // It's a patch.
        else
        {
          end1 = oldPrim->GetEnd1();
          end2 = oldPrim->GetEnd2();
          end3 = ((Patch*)oldPrim)->GetEnd3();
          end4 = ((Patch*)oldPrim)->GetEnd4();
          newPatch = new Patch("SP", end1, end2, end3, end4, 0, 0, 0);
          newPatch->Rotate(ang);
          newPatch->Move(pos);
          compareModule(newPatch->CalculateMaxModule());
          primitiveList.insert((j+index),newPatch);
        }
      }
    }
  }
}

void NECInput::processGXCard(int index)
{
  Line* newLine;
  Patch* newPatch;
  Primitive* oldPrim;
  int inc=1;

  bool x = false;
  bool y = false;
  bool z = false;

  QVector<double> end1;
  QVector<double> end2;
  QVector<double> end3;
  QVector<double> end4;

  int its = primitiveList.at(index)->GetTagNumber();
  int reflect = primitiveList.at(index)->GetCardParameter();
  primitiveList.removeAt(index);

  if(reflect>=100)
  {
    x = true;
    reflect = reflect-100;
  }
  if(reflect>=10)
  {
    y = true;
    reflect = reflect -10;
  }
  if(reflect==1)
  {
    z = true;
  }
  if(z)
  {
    for (int i=0; i<index; i++)
    {
      oldPrim = primitiveList.at(i);
      if(oldPrim->GetLabel()=="GW")		// It's a line.
      {
        end1 = oldPrim->GetEnd1();
        end2 = oldPrim->GetEnd2();
        newLine = new Line("GW", end1, end2, oldPrim->GetTagNumber()+its,0,0);
        newLine->Reflect(false, false, z);
        primitiveList.insert((i+index),newLine);
      }
      // It's a patch.
      else
      {
        end1 = oldPrim->GetEnd1();
        end2 = oldPrim->GetEnd2();
        end3 = ((Patch*)oldPrim)->GetEnd3();
        end4 = ((Patch*)oldPrim)->GetEnd4();
        newPatch = new Patch("SP", end1, end2, end3, end4, 0, 0, 0);
        newPatch->Reflect(false, false, z);
        primitiveList.insert((i+index),newPatch);
      }

    }
    inc=2*inc;
  }
  if(y)
  {
    for (int i=0; i<inc*index; i++)
    {
      oldPrim = primitiveList.at(i);
      if(oldPrim->GetLabel()=="GW")		// It's a line.
      {
        end1 = oldPrim->GetEnd1();
        end2 = oldPrim->GetEnd2();
        newLine = new Line("GW", end1, end2, oldPrim->GetTagNumber()+its,0,0);
        newLine->Reflect(false, false, z);
        primitiveList.insert((i+inc*index),newLine);
      }
      // It's a patch.
      else
      {
        end1 = oldPrim->GetEnd1();
        end2 = oldPrim->GetEnd2();
        end3 = ((Patch*)oldPrim)->GetEnd3();
        end4 = ((Patch*)oldPrim)->GetEnd4();
        newPatch = new Patch("SP", end1, end2, end3, end4, 0, 0, 0);
        newPatch->Reflect(false, y, false);
        primitiveList.insert((i+inc*index),newPatch);
      }
    }
    inc = 2*inc;
  }
  if(x)
  {
    for (int i=0; i<inc*index; i++)
    {
      oldPrim = primitiveList.at(i);
      if(oldPrim->GetLabel()=="GW")		// It's a line.
      {
        end1 = oldPrim->GetEnd1();
        end2 = oldPrim->GetEnd2();
        newLine = new Line("GW", end1, end2, oldPrim->GetTagNumber()+its,0,0);
        newLine->Reflect(false, false, z);
        primitiveList.insert((i+inc*index),newLine);
      }
      // It's a patch.
      else
      {
        end1 = oldPrim->GetEnd1();
        end2 = oldPrim->GetEnd2();
        end3 = ((Patch*)oldPrim)->GetEnd3();
        end4 = ((Patch*)oldPrim)->GetEnd4();
        newPatch = new Patch("SP", end1, end2, end3, end4, 0, 0, 0);
        newPatch->Reflect(x, false, false);
        primitiveList.insert((i+inc*index),newPatch);
      }
    }
  }
}

void NECInput::processGACard(int index)
{
  double rada, ang1, ang2, rad, step, point;
  Line* newLine;
  QVector<double> end1;
  QVector<double> end2;
  QVector<double> temp;

  // Initialise the vectors so we don't get a null index exception
  end1.append(0.0); end1.append(0.0); end1.append(0.0);
  end2.append(0.0); end2.append(0.0); end2.append(0.0);


  int tag = primitiveList.at(index)->GetTagNumber();
  int ns = primitiveList.at(index)->GetCardParameter();

  temp = primitiveList.at(index)->GetEnd1();
  rada = temp[0];
  ang1 = temp[1];
  ang2 = temp[2];

  temp = primitiveList.at(index)->GetEnd2();
  rad = temp[0];
  ang1 = ang1*3.141592654/180;
  ang2 = ang2*3.141592654/180;
  step = (ang2 - ang1)/ns;
  point = ang1;
  primitiveList.removeAt(index);

  for(int i=0; i<ns; i++)
  {
    end1[0] = rada * cos(point);
    end1[1] = 0.0;
    end1[2] = rada * sin(point);

    end2[0] = rada * cos(point+step);
    end2[1] = 0.0;
    end2[2] = rada * sin(point+step);

    point = point + step;
    newLine = new Line("GW", end1, end2, tag, 0, 0); // the Line class expects a 3-vector
    compareModule(newLine->CalculateMaxModule());
    primitiveList.insert((index+i), newLine);
  }
}

void NECInput::processSPCard(int index)
{
  Patch* newPatch;

  SPCard * spcard = 0;
  // Patch shape
  int ns;
  spcard = (SPCard*)cardsList.at(index);
  ns = spcard->getPatchShape();

  // Patch position.
  QVector<double> pos;
  QVector<double> ang;

  switch(ns)
  {
    // Arbitrary patch shape.
    case 0:
      // Angle from X axis.
      double alpha;
      // Elevation above X-Y plane.
      double phi;
      double edge;

      newPatch = new Patch();
      pos.append(spcard->getXCoordinateCorner1());
      pos.append(spcard->getYCoordinateCorner1());
      pos.append(spcard->getZCoordinateCorner1());
      phi    = spcard->getXCoordinateCorner2();
      alpha  = spcard->getYCoordinateCorner2();
      // We calculate the patch edge
      edge   = sqrt(spcard->getZCoordinateCorner2());
      newPatch = new Patch();
      newPatch->SetLabel("SP");
      newPatch->SetCardParameter(ns);
      newPatch->SetEnd1( edge/2, edge/2, 0);
      newPatch->SetEnd2(-edge/2, edge/2, 0);
      newPatch->SetEnd3(-edge/2, -edge/2, 0);
      newPatch->SetEnd4( edge/2, -edge/2, 0);
      ang.append(0.0);
      ang.append(-phi-90);
      ang.append(0.0);
      newPatch->Rotate(ang);
      ang.append(0.0);
      ang.append(0.0);
      ang.append(alpha);
      newPatch->Rotate(ang);
      newPatch->Move(pos);
      compareModule(newPatch->CalculateMaxModule());
      primitiveList.append(newPatch);
      break;

    // Rectangular patch shape.
    case 1:
      if((index+1)<cardsList.size())
      {
        // We need the next element.
        GenericCard * nextCard = 0;
        nextCard = cardsList.at(index+1);
        if(nextCard->getCardType() == "SC")
        {
          SCCard * sccard = (SCCard*)cardsList.at(index+1);
          Patch* newPatch;
          newPatch = new Patch;
          newPatch->SetLabel("SP");
          newPatch->SetCardParameter(ns);
          newPatch->SetEnd1(spcard->getXCoordinateCorner1(),
                            spcard->getYCoordinateCorner1(),
                            spcard->getZCoordinateCorner1());
          newPatch->SetEnd2(spcard->getXCoordinateCorner2(),
                            spcard->getYCoordinateCorner2(),
                            spcard->getZCoordinateCorner2());
          newPatch->SetEnd3(sccard->getXCoordinateCorner3(),
                            sccard->getYCoordinateCorner3(),
                            sccard->getZCoordinateCorner3());
          newPatch->SetEnd4(spcard->getXCoordinateCorner1() +
                            sccard->getXCoordinateCorner3() -
                            spcard->getXCoordinateCorner2(),
                            spcard->getYCoordinateCorner1() +
                            sccard->getYCoordinateCorner3() -
                            spcard->getXCoordinateCorner2(),
                            spcard->getZCoordinateCorner1() +
                            sccard->getZCoordinateCorner3() -
                            spcard->getZCoordinateCorner2());
          compareModule(newPatch->CalculateMaxModule());
          primitiveList.append(newPatch);
        }
      }
      break;

    // Triangular patch shape.
    case 2:
      if((index+1)<cardsList.size())
      {
        // We need the next element.
        GenericCard * nextElem;
        nextElem = cardsList.at(index+1);
        if(nextElem->getCardType() == "SC")
        {
          SCCard * sccard = (SCCard*)cardsList.at(index+1);
          Patch* newPatch;
          newPatch = new Patch;
          newPatch->SetLabel("SP");
          newPatch->SetCardParameter(ns);
          newPatch->SetEnd1(spcard->getXCoordinateCorner1(),
                            spcard->getYCoordinateCorner1(),
                            spcard->getZCoordinateCorner1());
          newPatch->SetEnd2(spcard->getXCoordinateCorner2(),
                            spcard->getYCoordinateCorner2(),
                            spcard->getZCoordinateCorner2());
          newPatch->SetEnd3(sccard->getXCoordinateCorner3(),
                            sccard->getYCoordinateCorner3(),
                            sccard->getZCoordinateCorner3());
          newPatch->SetEnd4( 0.0, 0.0, 0.0);
          compareModule(newPatch->CalculateMaxModule());
          primitiveList.append(newPatch);
        }
      }
      break;

      // Quadrilateral patch shape.
    case 3:
      if((index+1)<cardsList.size())
      {
        // We need the next element.
        GenericCard * nextElem;
        nextElem = cardsList.at(index+1);
        if(nextElem->getCardType() == "SC")
        {
          SCCard * sccard = (SCCard*)cardsList.at(index+1);
          Patch* newPatch;
          newPatch = new Patch;
          newPatch->SetLabel("SP");
          newPatch->SetCardParameter(ns);
          newPatch->SetEnd1(spcard->getXCoordinateCorner1(),
                            spcard->getYCoordinateCorner1(),
                            spcard->getZCoordinateCorner1());
          newPatch->SetEnd2(spcard->getXCoordinateCorner2(),
                            spcard->getYCoordinateCorner2(),
                            spcard->getZCoordinateCorner2());
          newPatch->SetEnd3(sccard->getXCoordinateCorner3(),
                            sccard->getYCoordinateCorner3(),
                            sccard->getZCoordinateCorner3());
          newPatch->SetEnd4(sccard->getXCoordinateCorner4(),
                            sccard->getYCoordinateCorner4(),
                            sccard->getZCoordinateCorner4());
          compareModule(newPatch->CalculateMaxModule());
          primitiveList.append(newPatch);
        }
      }
      break;
  }
}

void NECInput::processData()
{
  // We clean the primitive list
  qDeleteAll(primitiveList);
  primitiveList.clear();

  Line* newLine = 0;

  // Some pointers
  GenericCard * card = 0;
  GMCard * gmcard = 0;
  GXCard * gxcard = 0;
  GRCard * grcard = 0;
  GWCard * gwcard = 0;
  GACard * gacard = 0;
  GNCard * gncard = 0;
  FRCard * frcard = 0;
  SPCard * spcard = 0;
  SMCard * smcard = 0;
  GECard * gecard = 0;

  QVector<double> end1;
  // We create three values
  end1.append(0.0);
  end1.append(0.0);
  end1.append(0.0);

  QVector<double> end2;
  // We create three values
  end2.append(0.0);
  end2.append(0.0);
  end2.append(0.0);

  for( int i=0; i<cardsList.size() ; i++)
  {
    card = cardsList.at(i);
    // Variable cleaning
    end1[0] = 0.0;
    end1[1] = 0.0;
    end1[2] = 0.0;
    end2[0] = 0.0;
    end2[1] = 0.0;
    end2[2] = 0.0;

    if(card->getCardType() == "GM")
    {
      /*
      In this card tagNumber contains the tag increment.
      CardParameter is the number of new structures created.
      CardParameter1 is the start of the transformed structure.
      End1 contains rotation angles and end2 the new position.
      */
      gmcard = (GMCard*)cardsList.at(i);
      end1[0] = gmcard->getXRotationAngle();
      end1[1] = gmcard->getYRotationAngle();
      end1[2] = gmcard->getZRotationAngle();

      end2[0] = gmcard->getXTranslation();
      end2[1] = gmcard->getYTranslation();
      end2[2] = gmcard->getZTranslation();

      newLine = new Line("GM", end1, end2,gmcard->getTagNumberIncrement(),
                         gmcard->getNumberOfNewStructures(),
                         (int)gmcard->getInitialTag());
      primitiveList.append(newLine);
      gmcard = 0;
    }
    else if(card->getCardType() == "GX")
    {
      /*
        In this card tagNumber contains tag increment.
        Card parameter indicates which are the reflexion planes.
      */
      gxcard = (GXCard*)cardsList.at(i);
      newLine = new Line("GX", end1, end2,gxcard->getTagNumberIncrement(),gxcard->getReflectionFlags(), 0);
      primitiveList.append(newLine);
      gxcard = 0;
    }
    // This card is trated as a particular case of GM card (see NEC2 tutorial).
    else if(card->getCardType() == "GR")
    {
      /*
        In this card tagNumber contains tag increment.
        CardParameter indicates how many new structures are built.

        Note: From NEC2 Manual..
        The GR card produces the same effect on the structure as a GM
        card if I2 on the GR card is equal to (NRPT+1) on the GM card and if
        ROZ on the GM card is equal to 360/(NRPT+1) degrees. If the GM card is
        used, however, the program will not be set to take advantage of symmetry.
      */
      grcard = (GRCard*)cardsList.at(i);
      double roz = 360/grcard->getNumberOfOcurrencies();
      end1[2] = roz;	// Rotation about Z axis (index counts from zero 0,1,2)
      newLine = new Line("GM", end1, end2, grcard->getTagNumberIncrement(), grcard->getNumberOfOcurrencies()-1, 0);
      // Unsure about NRPT ever being allowed to be negative - (Graham)
      primitiveList.append(newLine);
      grcard = 0;
    }
    else if (card->getCardType() == "GW")
    {
      gwcard = (GWCard*)cardsList.at(i);

      end1[0] = gwcard->getXWire1();
      end1[1] = gwcard->getYWire1();
      end1[2] = gwcard->getZWire1();

      end2[0] = gwcard->getXWire2();
      end2[1] = gwcard->getYWire2();
      end2[2] = gwcard->getZWire2();

      newLine = new Line("GW", end1, end2, gwcard->getTagNumber(), 0, 0 );
      compareModule(newLine->CalculateMaxModule());
      primitiveList.append(newLine);
      gwcard = 0;
    }
    else if (card->getCardType() == "GA")
    {
      // End1 and End2 contain GA parameters.
      gacard = (GACard*)cardsList.at(i);
      end1[0] = gacard->getArcRadius();
      end1[1] = gacard->getFirstEndAngle();
      end1[2] = gacard->getSecondEndAngle();
      end2[0] = gacard->getWireRadius();

      newLine = new Line("GA", end1, end2, gacard->getTagNumber(),gacard->getNumberOfSegments(), 0 );
      primitiveList.append(newLine);
      gacard = 0;
    }
    else if(card->getCardType() == "GN")
    {
      gncard = (GNCard*)cardsList.at(i);

      // Radial wires begins always in the same place.
      end1[0] = 0.0;
      end1[1] = 0.0;
      end1[2] = 0.0;

      // Get how many lines we have to draw.
      int numberOfRadialWires = gncard->getNumberOfRadialWires();
      double angle = 2*PI/(double)numberOfRadialWires;

      for(int step=0; step < numberOfRadialWires; step++)
      {
        end2[0] = 10*cos(angle*step);
        end2[1] = 10*sin(angle*step);
        end2[2] = 0.0;
        newLine = new Line("GN", end1, end2, 0, 0, 0);
        primitiveList.append(newLine);
      }

      gncard = 0;
      newLine = 0;
    }
    else if (card->getCardType() == "SP")
    {
      processSPCard(i);
    }
    else if (card->getCardType() == "FR")
    {
      // We search for the first FR card
      if(!foundFRCard)
      {
        frcard = (FRCard*)cardsList.at(i);
        frequency = frcard->getFrequency();
        frcard = 0;
        foundFRCard = true;
      }
      /*
        If we already found a FR card, we fill the others with the same
        frequency of the first one, wich is just a side effect.
        If we are re-calcuating, we set the above frequency, which should be
        already NECContainer's frequency.
      */

      // Umm.. Some antenna arrays do have multiple exitation elements,
      // and they are intentionally at different frequencies  :|
      else
      {
        frcard = (FRCard*)cardsList.at(i);
        frcard->setFrequency(frequency);
        frcard = 0;
      }
    }
    else if (card->getCardType() == "SM")
    {
      // It's a particular case of SP with ns=1 (rectangular shape)
      smcard = (SMCard*)cardsList.takeAt(i);
      spcard = new SPCard(1,smcard->getXCoordinateCorner1(),
                          smcard->getYCoordinateCorner1(),
                          smcard->getZCoordinateCorner1(),
                          smcard->getXCoordinateCorner2(),
                          smcard->getYCoordinateCorner2(),
                          smcard->getZCoordinateCorner2());
      // We replace the SM card with the SP card
      cardsList.replace(i,(GenericCard*)spcard);
      // We finally delete the SM card
      delete smcard;
      smcard = 0;
      // And we process the new SP card
      processSPCard(i);
    }
    else if (card->getCardType() == "GE")
    {
      /*
        It's a particular case of SP with ns=1 (rectangular shape).
        As it may define the presence of the ground plane, it suffices to
        set it on if necessary.
      */
      gecard = (GECard*)cardsList.at(i);
      if(gecard->getGeometryGroundPlane() != 0)
        groundPlane = true;
      gecard = 0;
    }
  }
  processPrimitive();
}

void NECInput::processPrimitive()
{
/*
First we search for the index where is the first card which have effects on the
previous ones. They can be: GM, GR and GX. Then we process primitiveList and
repeat the cicle until there aren't any of these cards.
*/
  int i = 0;
  while(i<primitiveList.size())
  {
    if(primitiveList.at(i)->GetLabel()=="GM")
    {
      processGMCard(i);
    }
    else if (primitiveList.at(i)->GetLabel()=="GX")
    {
      processGXCard(i);
    }
    else if (primitiveList.at(i)->GetLabel()=="GA")
    {
      processGACard(i);
    }
    else
    {
      i++;
    }
  }
  createOpenGLList();
}

void NECInput::render()
{

  // We need to do depth testing
  glEnable(GL_DEPTH_TEST);

  // We enable the arrays
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_COLOR_ARRAY);

  glLineWidth(radius);

  glVertexPointer(3,GL_DOUBLE,0,linesVertexArray.data());
  glColorPointer(4,GL_DOUBLE,0,linesColorArray.data());
  glDrawArrays(GL_LINES,0,linesVertexArray.size()/3);

  glVertexPointer(3,GL_DOUBLE,0,trianglesVertexArray.data());
  glColorPointer(4,GL_DOUBLE,0,trianglesColorArray.data());
  glDrawArrays(GL_TRIANGLES,0,trianglesVertexArray.size()/3);

  glVertexPointer(3,GL_DOUBLE,0,quadsVertexArray.data());
  glColorPointer(4,GL_DOUBLE,0,quadsColorArray.data());
  glDrawArrays(GL_QUADS,0,quadsVertexArray.size()/3);

  // I think this is a horrible hack ¿Is there any glSomething to do this?
  glLineWidth(1);

    // We disable the arrays
  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_COLOR_ARRAY);

  // We disable depth testing
  glDisable(GL_DEPTH_TEST);
}

QString NECInput::getFileName()
{
  return fileName;
}

void NECInput::createOpenGLList()
{
  GLUquadricObj *quad;
  QVector<double> temp;

  // We clean the arrays
  linesVertexArray.clear();
  linesColorArray.clear();
  quadsVertexArray.clear();
  quadsColorArray.clear();
  trianglesVertexArray.clear();
  trianglesColorArray.clear();

  for(int i=0; i<primitiveList.size(); i++)
  {
    if(primitiveList.at(i)->GetLabel()=="GW")
    {
      if (primitiveList.at(i)->GetTagNumber()==radiatingElement)
      {
        for(int j=0; j<2; j++)
        {
          linesColorArray.append(1.0);
          linesColorArray.append(0.0);
          linesColorArray.append(0.0);
          linesColorArray.append(0.0);
        }
      }
      else
      {
        for(int j=0; j<2; j++)
        {
          linesColorArray.append(0.5);
          linesColorArray.append(0.0);
          linesColorArray.append(0.5);
          linesColorArray.append(1.0);
        }
      }

      temp = primitiveList.at(i)->GetEnd1();
      transformate(temp);
      linesVertexArray.append(temp.at(0));
      linesVertexArray.append(temp.at(1));
      linesVertexArray.append(temp.at(2));

      temp = primitiveList.at(i)->GetEnd2();
      transformate(temp);
      linesVertexArray.append(temp.at(0));
      linesVertexArray.append(temp.at(1));
      linesVertexArray.append(temp.at(2));
    }
    else if(primitiveList.at(i)->GetLabel()=="SP")
    {
      if(primitiveList.at(i)->GetCardParameter()==2)
      {
        // We give the colours
        for(int j=0; j<3; j++)
        {
          trianglesColorArray.append(0.5);
          trianglesColorArray.append(0.5);
          trianglesColorArray.append(0.5);
          trianglesColorArray.append(1.0);
        }

        temp = primitiveList.at(i)->GetEnd1();
        transformate(temp);
        trianglesVertexArray.append(temp.at(0));
        trianglesVertexArray.append(temp.at(1));
        trianglesVertexArray.append(temp.at(2));

        temp = primitiveList.at(i)->GetEnd2();
        transformate(temp);
        trianglesVertexArray.append(temp.at(0));
        trianglesVertexArray.append(temp.at(1));
        trianglesVertexArray.append(temp.at(2));

        temp = ((Patch*)primitiveList.at(i))->GetEnd3();
        transformate(temp);
        trianglesVertexArray.append(temp.at(0));
        trianglesVertexArray.append(temp.at(1));
        trianglesVertexArray.append(temp.at(2));
      }
      else
      {
        // We give the colors
        for(int j=0; j<4; j++)
        {
          quadsColorArray.append(1.0);
          quadsColorArray.append(1.0);
          quadsColorArray.append(1.0);
          quadsColorArray.append(1.0);
        }

        temp = primitiveList.at(i)->GetEnd1();
        transformate(temp);
        quadsVertexArray.append(temp.at(0));
        quadsVertexArray.append(temp.at(1));
        quadsVertexArray.append(temp.at(2));

        temp = primitiveList.at(i)->GetEnd2();
        transformate(temp);
        quadsVertexArray.append(temp.at(0));
        quadsVertexArray.append(temp.at(1));
        quadsVertexArray.append(temp.at(2));

        temp = ((Patch*)primitiveList.at(i))->GetEnd3();
        transformate(temp);
        quadsVertexArray.append(temp.at(0));
        quadsVertexArray.append(temp.at(1));
        quadsVertexArray.append(temp.at(2));

        temp = ((Patch*)primitiveList.at(i))->GetEnd4();
        transformate(temp);
        quadsVertexArray.append(temp.at(0));
        quadsVertexArray.append(temp.at(1));
        quadsVertexArray.append(temp.at(2));
      }
    }
    if(primitiveList.at(i)->GetLabel()=="GN")
    {
      // Add colour
      linesColorArray.append(0.5);
      linesColorArray.append(0.3);
      linesColorArray.append(0.0);
      linesColorArray.append(1.0);

      linesColorArray.append(0.5);
      linesColorArray.append(0.3);
      linesColorArray.append(0.0);
      linesColorArray.append(1.0);

      temp = primitiveList.at(i)->GetEnd1();
      transformate(temp);
      linesVertexArray.append(temp.at(0));
      linesVertexArray.append(temp.at(1));
      linesVertexArray.append(temp.at(2));

      temp = primitiveList.at(i)->GetEnd2();
      transformate(temp);
      linesVertexArray.append(temp.at(0));
      linesVertexArray.append(temp.at(1));
      linesVertexArray.append(temp.at(2));
    }
  }
  ///FIXME We are not getting ground planes :-/
  if(groundPlane)
  {
    quad = gluNewQuadric();
    glPushMatrix();
    glColor3f(0.78, 0.96, 1);
    glRotatef( 90, 1, 0, 0);
    // In radius, out radius, slices and loops.
    gluDisk( quad, 0, 1.5, 30, 15);
    glPopMatrix();
  }
  glEndList();
}

void NECInput::compareModule(double module)
{
  if(module > maxModule)
    maxModule = module;
}

void NECInput::transformate(QVector<double> & end)
{	// First we normalize the point.

  end[0] = end[0]/maxModule;
  end[1] = end[1]/maxModule;
  end[2] = end[2]/maxModule;

  double temp;
// Then we change the axis so that OpenGL showed them well.
  temp = end[2];
  end[2] = -1*end[1];
  end[1] = temp;
}

void NECInput::createNECInputFile()
{
  QString thePath = QDir::tempPath();
  /*
    We clean the path, given us a path without a final separator, no matter
    which OS we are running (some OSs give it and some doesn't)
  */
  thePath = QDir::cleanPath(thePath);
  // We append a final separator
  thePath.append("/");

  thePath.append("input" + creationTime + ".necin");
  QFile theFile(thePath);

  if (theFile.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    QTextStream textStream(&theFile);
    for(int i=0; i<cardsList.size(); i++)
      textStream << cardsList.at(i)->getCard();
    // Everything that is opened must be closed before leaving
    theFile.close();
  }
  else
  {
    qDebug() << "NECInput::createNECInputFile";
    qDebug() << QWidget::tr("I could not create/open input.necin");
  }
}

