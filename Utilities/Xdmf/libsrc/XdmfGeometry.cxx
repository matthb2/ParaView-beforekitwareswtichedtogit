/*******************************************************************/
/*                               XDMF                              */
/*                   eXtensible Data Model and Format              */
/*                                                                 */
/*  Id : $Id$  */
/*  Date : $Date$ */
/*  Version : $Revision$ */
/*                                                                 */
/*  Author:                                                        */
/*     Jerry A. Clarke                                             */
/*     clarke@arl.army.mil                                         */
/*     US Army Research Laboratory                                 */
/*     Aberdeen Proving Ground, MD                                 */
/*                                                                 */
/*     Copyright @ 2002 US Army Research Laboratory                */
/*     All Rights Reserved                                         */
/*     See Copyright.txt or http://www.arl.hpc.mil/ice for details */
/*                                                                 */
/*     This software is distributed WITHOUT ANY WARRANTY; without  */
/*     even the implied warranty of MERCHANTABILITY or FITNESS     */
/*     FOR A PARTICULAR PURPOSE.  See the above copyright notice   */
/*     for more information.                                       */
/*                                                                 */
/*******************************************************************/
#include "XdmfGeometry.h"

#include "XdmfTransform.h"
#include "XdmfFormatMulti.h"
#include "XdmfArray.h"
#include "XdmfDOM.h"
#include "XdmfHDF.h"

XdmfGeometry *GetXdmfGeometryHandle( void *Pointer ){
  //XdmfGeometry *tmp = (XdmfGeometry *)Pointer;
  return((XdmfGeometry *)Pointer);
  }

XdmfGeometry::XdmfGeometry() {
  this->GeometryType = XDMF_GEOMETRY_NONE;
  this->Points = NULL;
  this->PointsAreMine = 1;
  this->VectorX = NULL;
  this->VectorY = NULL;
  this->VectorZ = NULL;
  this->SetOrigin( 0, 0, 0 );
  this->SetDxDyDz( 0, 0, 0 );
  }

XdmfGeometry::~XdmfGeometry() {
  if( this->PointsAreMine && this->Points )  delete Points;
  }

XdmfInt32
XdmfGeometry::SetOrigin( XdmfFloat64 X, XdmfFloat64 Y, XdmfFloat64 Z ){

this->Origin[0] = X;
this->Origin[1] = Y;
this->Origin[2] = Z;
return( XDMF_SUCCESS );
}

XdmfInt32
XdmfGeometry::SetOrigin( XdmfFloat64 *Origin ){
return( this->SetOrigin( Origin[0], Origin[1], Origin[2] ) );
}

XdmfInt32
XdmfGeometry::SetDxDyDz( XdmfFloat64 Dx, XdmfFloat64 Dy, XdmfFloat64 Dz ){
this->DxDyDz[0] = Dx;
this->DxDyDz[1] = Dy;
this->DxDyDz[2] = Dz;
return( XDMF_SUCCESS );
}


XdmfInt32
XdmfGeometry::SetDxDyDz( XdmfFloat64 *DxDyDz ){
return( this->SetDxDyDz( DxDyDz[0], DxDyDz[1], DxDyDz[2] ) );
}


XdmfInt32
XdmfGeometry::SetPoints( XdmfArray *Points ){
    if( this->PointsAreMine && this->Points ) delete Points;
    this->PointsAreMine = 0;
    this->Points = Points;
    return( XDMF_SUCCESS );
    }

XdmfInt32
XdmfGeometry::SetGeometryTypeFromString( XdmfString GeometryType ){

if( XDMF_WORD_CMP( GeometryType, "X_Y_Z" ) ){
  this->GeometryType = XDMF_GEOMETRY_X_Y_Z;
  return(XDMF_SUCCESS);
  }
if( XDMF_WORD_CMP( GeometryType, "XY" ) ){
  this->GeometryType = XDMF_GEOMETRY_XY;
  return(XDMF_SUCCESS);
  }
if( XDMF_WORD_CMP( GeometryType, "XYZ" ) ){
  this->GeometryType = XDMF_GEOMETRY_XYZ;
  return(XDMF_SUCCESS);
  }
if( XDMF_WORD_CMP( GeometryType, "ORIGIN_DXDYDZ" ) ){
  this->GeometryType = XDMF_GEOMETRY_ORIGIN_DXDYDZ;
  return(XDMF_SUCCESS);
  }
if( XDMF_WORD_CMP( GeometryType, "VXVYVZ" ) ){
  this->GeometryType = XDMF_GEOMETRY_VXVYVZ;
  return(XDMF_SUCCESS);
  }
return( XDMF_FAIL );
}

XdmfString
XdmfGeometry::GetGeometryTypeAsString( void ){
static char Value[80];

switch( this->GeometryType ){
  case XDMF_GEOMETRY_VXVYVZ:
    strcpy( Value, "VXVYVZ" );
    break;
  case XDMF_GEOMETRY_ORIGIN_DXDYDZ:
    strcpy( Value, "ORIGIN_DXDYDZ" );
    break;
  case XDMF_GEOMETRY_X_Y_Z :
    strcpy( Value, "X_Y_Z" );
    break;
  case XDMF_GEOMETRY_XY :
    strcpy( Value, "XY" );
    break;
  default :
    strcpy( Value, "XYZ" );
    break;
  }
return( Value );
}

XdmfInt32
XdmfGeometry::InitGeometryFromElement( XdmfXNode *Element ) {
XdmfString  Attribute;

if( !Element ) {
  XdmfErrorMessage("Element is NULL");
  return( XDMF_FAIL );
  }
Attribute = this->DOM->Get( Element, "NodeType");
if( XDMF_WORD_CMP( Attribute, "Geometry" ) == 0 ){
  Element = this->DOM->FindElement( "Geometry", 0, Element );
  if( !Element ){
    XdmfErrorMessage("Can't find Geometry Element");
    return( XDMF_FAIL );
    }
  }
Attribute = this->DOM->Get( Element, "Type" );
if( Attribute ){
  this->SetGeometryTypeFromString( Attribute );
} else {
  this->GeometryType = XDMF_GEOMETRY_XYZ;
}

Attribute = this->DOM->Get( Element, "Name" );
if( Attribute ) {
  this->SetName( Attribute );
} else {
  this->SetName( GetUnique("Geometry_" ) );
}
this->CurrentElement = Element;
return( XDMF_SUCCESS );
}

XdmfInt32
XdmfGeometry::SetGeometryFromElement( XdmfXNode *Element ) {

XdmfString  Attribute;
XdmfInt32  ArrayIndex;
XdmfInt64  Start[ XDMF_MAX_DIMENSION ];
XdmfInt64  Stride[ XDMF_MAX_DIMENSION ];
XdmfInt64  Count[ XDMF_MAX_DIMENSION ];
XdmfXNode    *PointsElement;
XdmfArray  *Points = NULL;
XdmfArray  *TmpArray;
XdmfTransform  PointsReader;

if( this->GeometryType == XDMF_GEOMETRY_NONE ){
  if( this->InitGeometryFromElement( Element ) == XDMF_FAIL ){
    XdmfErrorMessage("Can't Initialize From Element");
    return( XDMF_FAIL );
  }
}
Attribute = this->DOM->Get( Element, "NodeType");
if( XDMF_WORD_CMP( Attribute, "Geometry" ) == 0 ){
  Element = this->DOM->FindElement( "Geometry", 0, Element );
  if( !Element ){
    XdmfErrorMessage("Can't find Geometry Element");
    return( XDMF_FAIL );
    }
  }
 ArrayIndex = 0;
 PointsReader.SetDOM( this->DOM );
if( ( this->GeometryType == XDMF_GEOMETRY_X_Y_Z ) ||
  ( this->GeometryType == XDMF_GEOMETRY_XYZ ) ||
  ( this->GeometryType == XDMF_GEOMETRY_XY ) ){
 do {
  // Read the Data
  XdmfDebug("Reading Points ( SubElement #" << ArrayIndex + 1 << " )" );
  PointsElement = this->DOM->FindElement(NULL, ArrayIndex, Element );
  if( PointsElement ){
    TmpArray = PointsReader.ElementToArray( PointsElement );
  if( TmpArray ){
    if( !Points ){
    switch( this->GeometryType ){
      case XDMF_GEOMETRY_X_Y_Z :
        Points = new XdmfArray;
        Points->CopyType( TmpArray );
        Points->SetNumberOfElements( TmpArray->GetNumberOfElements() * 3 );
        break;
      case XDMF_GEOMETRY_XY :
        Points = new XdmfArray;
        Points->CopyType( TmpArray );
        Points->SetNumberOfElements( TmpArray->GetNumberOfElements() / 2 * 3 );
        break;
      default :
        Points = TmpArray;
        break;
      }
    }
    // We Have made Points Rank = 1 if not XYZ
    switch( this->GeometryType ){
      case XDMF_GEOMETRY_X_Y_Z :
        Start[0] = ArrayIndex;
        Stride[0] = 3;
        Points->SelectHyperSlab( Start, Stride, NULL );
        CopyArray( TmpArray, Points);
        this->NumberOfPoints = TmpArray->GetNumberOfElements();
        break;
      case XDMF_GEOMETRY_XY :
        Start[0] = TmpArray->GetNumberOfElements();
        Start[1] = 3;
        Points->SetShape( 2 , Start );
        Stride[0] = 1;
        Stride[0] = 1;
        Count[0] = TmpArray->GetNumberOfElements();
        Count[1] = 2;
        Points->SelectHyperSlab( NULL, Stride, Count);
        CopyArray( TmpArray, Points);
        this->NumberOfPoints = TmpArray->GetNumberOfElements() / 2 ;
        break;
      default :
        // Points = TmpArray so do nothing
        this->NumberOfPoints = TmpArray->GetNumberOfElements() / 3;
        break;
      }
  }
  } 
  ArrayIndex++;
 } while( ( ArrayIndex < 3 ) && ( PointsElement != NULL ) );
} else {
  if( this->GeometryType == XDMF_GEOMETRY_ORIGIN_DXDYDZ ) {
      XdmfDebug("Reading Origin and Dx, Dy, Dz" );
      PointsElement = this->DOM->FindElement(NULL, 0, Element );
      if( PointsElement ){
        TmpArray = PointsReader.ElementToArray( PointsElement );
      if( TmpArray ){
        TmpArray->GetValues( 0, this->Origin, 3 );
        delete TmpArray;
      }
        PointsElement = this->DOM->FindElement(NULL, 1, Element );
        if( PointsElement ){
          TmpArray = PointsReader.ElementToArray( PointsElement );
        if( TmpArray ){
          TmpArray->GetValues( 0, this->DxDyDz, 3 );
          delete TmpArray;
        }
      } else {
        XdmfErrorMessage("No Dx, Dy, Dz Specified");
        return( XDMF_FAIL );
      }
    } else {
      XdmfErrorMessage("No Origin Specified");
      return( XDMF_FAIL );
    }
  } else if( this->GeometryType == XDMF_GEOMETRY_VXVYVZ ) {
      XdmfDebug("Reading VectorX, VectorY, VectorZ" );
      PointsElement = this->DOM->FindElement(NULL, 0, Element );
      if( PointsElement ){
        TmpArray = PointsReader.ElementToArray( PointsElement );
      this->VectorX = TmpArray;
    } else {
      XdmfErrorMessage("No VectorX Specified");
      return( XDMF_FAIL );
      }
      PointsElement = this->DOM->FindElement(NULL, 1, Element );
      if( PointsElement ){
        TmpArray = PointsReader.ElementToArray( PointsElement );
      this->VectorY = TmpArray;
    } else {
      XdmfErrorMessage("No VectorY Specified");
      return( XDMF_FAIL );
      }
      PointsElement = this->DOM->FindElement(NULL, 2, Element );
      if( PointsElement ){
        TmpArray = PointsReader.ElementToArray( PointsElement );
      this->VectorZ = TmpArray;
    } else {
      XdmfErrorMessage("No VectorZ Specified");
      return( XDMF_FAIL );
      }
  }
}
if( Points ) this->SetPoints( Points );
this->CurrentElement = Element;
return( XDMF_SUCCESS );
}

XdmfInt32
XdmfGeometry::Update( void ) {

if( this->DOM && this->CurrentElement ) {
  return( this->SetGeometryFromElement( this->CurrentElement ) );
}
XdmfErrorMessage("No Element has been set in DOM");
return( XDMF_FAIL );
}
