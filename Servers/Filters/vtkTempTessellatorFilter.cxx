/*
 * Copyright 2003 Sandia Corporation.
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the
 * U.S. Government. Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that this Notice and any
 * statement of authorship are reproduced on all copies.
 */
#include <vtkObjectFactory.h>
#include <vtkCellType.h>
#include <vtkPoints.h>
#include <vtkDataSetAttributes.h>
#include <vtkPointData.h>
#include <vtkFieldData.h>
#include <vtkDataArray.h>
#include <vtkFloatArray.h>
#include <vtkDataSet.h>
#include <vtkCell.h>

#include <vtkTempTessellatorFilter.h>
#include <vtkDataSet.h>
#include <vtkUnstructuredGrid.h>
#include <vtkStreamingTessellator.h>
#include <vtkDataSetSubdivisionAlgorithm.h>
#include <vtkSubdivisionAlgorithm.h>

vtkCxxRevisionMacro(vtkTempTessellatorFilter, "$Revision$");
vtkStandardNewMacro(vtkTempTessellatorFilter);

// ========================================
// convenience routines for paraview
void vtkTempTessellatorFilter::SetMaximumNumberOfSubdivisions( int N )
{
  if ( this->Tessellator )
    this->Tessellator->SetMaximumNumberOfSubdivisions( N );
}

int vtkTempTessellatorFilter::GetMaximumNumberOfSubdivisions()
{
  return this->Tessellator ? this->Tessellator->GetMaximumNumberOfSubdivisions() : 0;
}

void vtkTempTessellatorFilter::SetChordError( double E )
{
  if ( this->Subdivider )
    this->Subdivider->SetChordError2( E > 0. ? E*E : E );
}

double vtkTempTessellatorFilter::GetChordError()
{
  double tmp = this->Subdivider ? this->Subdivider->GetChordError2() : 0.;
  return tmp > 0. ? sqrt( tmp ) : tmp;
}

void vtkTempTessellatorFilter::SetMergePoints( int DoTheMerge )
{
  if ( DoTheMerge == this->MergePoints )
    return;

  this->MergePoints = DoTheMerge;
  this->Modified();
}

// ========================================
// callbacks for simplex output
void vtkTempTessellatorFilter::AddATetrahedron( const double* a, const double* b, const double* c, const double* d,
                                              vtkSubdivisionAlgorithm*, void* pd, const void* )
{
  vtkTempTessellatorFilter* self = (vtkTempTessellatorFilter*) pd;
  self->OutputTetrahedron( a, b, c, d );
}

void vtkTempTessellatorFilter::OutputTetrahedron( const double* a, const double* b, const double* c, const double* d )
{
  vtkIdType cellIds[4];

  cellIds[0] = this->OutputPoints->InsertNextPoint( a );
  cellIds[1] = this->OutputPoints->InsertNextPoint( b );
  cellIds[2] = this->OutputPoints->InsertNextPoint( c );
  cellIds[3] = this->OutputPoints->InsertNextPoint( d );

  this->OutputMesh->InsertNextCell( VTK_TETRA, 4, cellIds );

  const int* off = this->Subdivider->GetFieldOffsets();
  vtkDataArray** att = this->OutputAttributes;

  // Move a, b, & c past the geometric and parametric coordinates to the
  // beginning of the field values.
  a += 6;
  b += 6;
  c += 6;
  d += 6;

  for ( int at=0; at<this->Subdivider->GetNumberOfFields(); ++at, ++att, ++off )
    {
    (*att)->InsertTuple( cellIds[0], a + *off );
    (*att)->InsertTuple( cellIds[1], b + *off );
    (*att)->InsertTuple( cellIds[2], c + *off );
    (*att)->InsertTuple( cellIds[3], d + *off );
    }
}

void vtkTempTessellatorFilter::AddATriangle( const double* a, const double* b, const double* c,
                                           vtkSubdivisionAlgorithm*, void* pd, const void* )
{
  vtkTempTessellatorFilter* self = (vtkTempTessellatorFilter*) pd;
  self->OutputTriangle( a, b, c );
}

void vtkTempTessellatorFilter::OutputTriangle( const double* a, const double* b, const double* c )
{
  vtkIdType cellIds[3];

  cellIds[0] = this->OutputPoints->InsertNextPoint( a );
  cellIds[1] = this->OutputPoints->InsertNextPoint( b );
  cellIds[2] = this->OutputPoints->InsertNextPoint( c );

  this->OutputMesh->InsertNextCell( VTK_TRIANGLE, 3, cellIds );

  const int* off = this->Subdivider->GetFieldOffsets();
  vtkDataArray** att = this->OutputAttributes;

  // Move a, b, & c past the geometric and parametric coordinates to the
  // beginning of the field values.
  a += 6;
  b += 6;
  c += 6;

  for ( int at=0; at<this->Subdivider->GetNumberOfFields(); ++at, ++att, ++off )
    {
    (*att)->InsertTuple( cellIds[0], a + *off );
    (*att)->InsertTuple( cellIds[1], b + *off );
    (*att)->InsertTuple( cellIds[2], c + *off );
    }
}

void vtkTempTessellatorFilter::AddALine( const double* a, const double* b,
                                       vtkSubdivisionAlgorithm*, void* pd, const void* )
{
  vtkTempTessellatorFilter* self = (vtkTempTessellatorFilter*) pd;
  self->OutputLine( a, b );
}

void vtkTempTessellatorFilter::OutputLine( const double* a, const double* b )
{
  vtkIdType cellIds[2];

  cellIds[0] = this->OutputPoints->InsertNextPoint( a );
  cellIds[1] = this->OutputPoints->InsertNextPoint( b );

  this->OutputMesh->InsertNextCell( VTK_LINE, 2, cellIds );

  const int* off = this->Subdivider->GetFieldOffsets();
  vtkDataArray** att = this->OutputAttributes;

  // Move a, b, & c past the geometric and parametric coordinates to the
  // beginning of the field values.
  a += 6;
  b += 6;

  for ( int at=0; at<this->Subdivider->GetNumberOfFields(); ++at, ++att, ++off )
    {
    (*att)->InsertTuple( cellIds[0], a + *off );
    (*att)->InsertTuple( cellIds[1], b + *off );
    }
}

// ========================================
// constructor/boilerplate members
vtkTempTessellatorFilter::vtkTempTessellatorFilter()
  : Tessellator( 0 ), Subdivider( 0 )
{
  this->OutputDimension = 3; // Tesselate elements directly, not boundaries
  this->SetTessellator( vtkStreamingTessellator::New() );
  this->SetSubdivider( vtkDataSetSubdivisionAlgorithm::New() );

  this->Tessellator->SetEmbeddingDimension( 1, 3 );
  this->Tessellator->SetEmbeddingDimension( 2, 3 );
}

vtkTempTessellatorFilter::~vtkTempTessellatorFilter()
{
  this->SetSubdivider( 0 );
  this->SetTessellator( 0 );
}

void vtkTempTessellatorFilter::PrintSelf( ostream& os, vtkIndent indent )
{
  this->Superclass::PrintSelf( os, indent );
  os << indent << "OutputDimension: " << this->OutputDimension << endl
     << indent << "Tessellator: " << this->Tessellator << endl
     << indent << "Subdivider: " << this->Subdivider << " (" << this->Subdivider->GetClassName() << ")" << endl;
}

// override for proper Update() behavior
unsigned long vtkTempTessellatorFilter::GetMTime()
{
  unsigned long mt = this->MTime;
  unsigned long tmp;

  if ( this->Tessellator )
    {
    tmp = this->Tessellator->GetMTime();
    if ( tmp > mt )
      mt = tmp;
    }

  if ( this->Subdivider )
    {
    tmp = this->Subdivider->GetMTime();
    if ( tmp > mt )
      mt = tmp;
    }

  return mt;
}

void vtkTempTessellatorFilter::SetTessellator( vtkStreamingTessellator* t )
{
  if ( this->Tessellator == t )
    return;

  if ( this->Tessellator )
    this->Tessellator->UnRegister( this );

  this->Tessellator = t;

  if ( this->Tessellator )
    {
    this->Tessellator->Register( this );
    this->Tessellator->SetSubdivisionAlgorithm( this->Subdivider );
    }

  this->Modified();
}

void vtkTempTessellatorFilter::SetSubdivider( vtkDataSetSubdivisionAlgorithm* s )
{
  if ( this->Subdivider == s )
    return;

  if ( this->Subdivider )
    this->Subdivider->UnRegister( this );

  this->Subdivider = s;

  if ( this->Subdivider )
    this->Subdivider->Register( this );

  if ( this->Tessellator )
    this->Tessellator->SetSubdivisionAlgorithm( this->Subdivider );

  this->Modified();
}

void vtkTempTessellatorFilter::SetFieldCriterion( int s, double err )
{
  if ( this->Subdivider )
    this->Subdivider->SetFieldError2( s, err > 0. ? err*err : -1. );
}

void vtkTempTessellatorFilter::ResetFieldCriteria()
{
  if ( this->Subdivider )
    this->Subdivider->ResetFieldError2();
}

// ========================================
// pipeline procedures
void vtkTempTessellatorFilter::SetupOutput()
{
  this->OutputMesh = this->GetOutput(); // avoid doing all the stupid checks on NumberOfOutputs for every triangle/line.
  this->OutputMesh->Reset();
  this->OutputMesh->Allocate(0,0);

  if ( ! (this->OutputPoints = OutputMesh->GetPoints()) )
    {
    this->OutputPoints = vtkPoints::New();
    this->OutputMesh->SetPoints( this->OutputPoints );
    this->OutputPoints->Delete();
    }

  int maxNumComponents = 0;

  // This returns the id numbers of arrays that are default scalars, vectors, normals, texture coords, and tensors.
  // These are the fields that will be interpolated and passed on to the output mesh.
  vtkPointData* fields = this->GetInput()->GetPointData();
  vtkDataSetAttributes* outarrays = this->OutputMesh->GetPointData();
  outarrays->Initialize(); // empty, turn off all attributes, and set CopyAllOn to true.

  this->OutputAttributes = new vtkDataArray* [ fields->GetNumberOfArrays() ];
  this->OutputAttributeIndices = new int [ fields->GetNumberOfArrays() ];

  // OK, we always add normals as the 0-th array so that there's less work to do inside the tight loop (OutputTriangle)
  int attrib = 0;
  for ( int a = 0; a < fields->GetNumberOfArrays(); ++a )
    {
    if ( fields->IsArrayAnAttribute( a ) == vtkDataSetAttributes::NORMALS )
      continue;

    vtkDataArray* array = fields->GetArray( a );
    this->OutputAttributes[ attrib ] = vtkDataArray::CreateDataArray( array->GetDataType() );
    this->OutputAttributes[ attrib ]->SetNumberOfComponents( array->GetNumberOfComponents() );
    this->OutputAttributes[ attrib ]->SetName( array->GetName() );
    this->OutputAttributeIndices[ attrib ] = outarrays->AddArray( this->OutputAttributes[ attrib ] );
    this->OutputAttributes[ attrib ]->Delete(); // output mesh now owns the array
    int attribType;
    if ( (attribType = fields->IsArrayAnAttribute( a )) != -1 )
      outarrays->SetActiveAttribute( this->OutputAttributeIndices[ attrib ], attribType );

    this->Subdivider->PassField( a, array->GetNumberOfComponents(), this->Tessellator );
    ++attrib;
    }
}

void vtkTempTessellatorFilter::Teardown()
{
  this->OutputMesh = 0;
  this->OutputPoints = 0;
  if ( this->OutputAttributes )
    delete [] this->OutputAttributes;
  if ( this->OutputAttributeIndices )
    delete [] this->OutputAttributeIndices;
  this->Subdivider->ResetFieldList();
}

// ========================================
// output element topology
double extraLinHexParams[12][3] =
{
  { 0.5, 0.0, 0.0 },
  { 1.0, 0.5, 0.0 },
  { 0.5, 1.0, 0.0 },
  { 0.0, 0.5, 0.0 },
  { 0.5, 0.0, 1.0 },
  { 1.0, 0.5, 1.0 },
  { 0.5, 1.0, 1.0 },
  { 0.0, 0.5, 1.0 },
  { 0.0, 0.5, 0.5 },
  { 0.5, 0.0, 0.5 },
  { 1.0, 0.5, 0.5 },
  { 0.5, 1.0, 0.5 },
};

double extraQuadHexParams[7][3] =
{
  { 0.5, 0.5, 0.0 },
  { 0.5, 0.5, 1.0 },
  { 0.5, 0.0, 0.5 },
  { 0.5, 1.0, 0.5 },
  { 0.0, 0.5, 0.5 },
  { 1.0, 0.5, 0.5 },
  { 0.5, 0.5, 0.5 }
};

double extraQuadQuadParams[1][3] =
{
  { 0.5, 0.5, 0.0 }
};

vtkIdType linEdgeEdges[][2] =
{
  {0,1}
};

vtkIdType quadEdgeEdges[][2] =
{
  {0,2},
  {2,1}
};

vtkIdType linTriTris[][3] =
{
  {0,1,2}
};

vtkIdType linTriEdges[][2] =
{
  {0,1},
  {1,2},
  {2,0}
};

vtkIdType quadTriTris[][3] =
{
  {0,3,5},
  {5,3,1},
  {5,1,4},
  {4,2,5}
};

vtkIdType quadTriEdges[][2] =
{
  {0,3},
  {3,1},
  {1,4},
  {4,2},
  {2,5},
  {5,0}
};

vtkIdType linQuadTris[][3] =
{
  {0,1,2},
  {0,2,3}
};

vtkIdType linQuadEdges[][2] =
{
  {0,1},
  {1,2},
  {2,3},
  {3,0}
};

vtkIdType quadQuadTris[][3] =
{
  {0,4,7},
  {7,4,8},
  {7,8,3},
  {3,8,6},
  {4,1,5},
  {8,4,5},
  {8,5,2},
  {2,6,8}
};

vtkIdType quadQuadEdges[][2] =
{
  {0,4},
  {4,1},
  {1,5},
  {5,2},
  {2,6},
  {6,3},
  {3,7},
  {7,0}
};

vtkIdType linWedgeTetrahedra[][4] =
{
  {3,2,1,0},
  {1,2,3,4},
  {2,3,4,5}
};

vtkIdType linWedgeTris[][3] =
{
  {0,2,1},
  {3,4,5},
  {3,4,5},
  {3,4,5},
  {0,1,3},
  {3,1,4},
  {1,2,4},
  {4,2,5},
  {2,0,5},
  {5,0,3}
};

vtkIdType linWedgeEdges[][2] =
{
  {0,1},
  {1,2},
  {2,0},
  {3,4},
  {4,5},
  {5,3},
  {0,3},
  {1,4},
  {2,5}
};

vtkIdType linPyrTetrahedra[][4] =
{
  {0,1,2,4},
  {0,2,3,4}
};

vtkIdType linPyrTris[][3] =
{
  {0,1,2},
  {0,2,3},
  {0,1,4},
  {1,2,4},
  {2,3,4},
  {3,0,4}
};

vtkIdType linPyrEdges[][2] =
{
  {0,1},
  {1,2},
  {2,3},
  {3,0},
  {0,4},
  {1,4},
  {2,4},
  {3,4}
};

vtkIdType linTetTetrahedra[][4] =
{
  {0,1,2,3}
};

vtkIdType linTetTris[][3] =
{
  {0,2,1},
  {0,1,3},
  {1,2,3},
  {2,0,3}
};

vtkIdType linTetEdges[][2] =
{
  {0,1},
  {1,2},
  {2,0},
  {0,3},
  {1,3},
  {2,3}
};

vtkIdType quadTetTetrahedra[][4] = 
{
  {4,7,6,0},
  {5,6,9,2},
  {7,8,9,3},
  {1,6,4,7},
  {1,6,7,5},
  {1,5,6,8},
  {5,6,8,9}
};

vtkIdType quadTetTris[][3] = 
{
  {0,6,4},
  {4,6,5},
  {5,6,2},
  {4,5,1},

  {0,4,7},
  {7,4,8},
  {8,4,1},
  {7,8,3},

  {1,5,8},
  {8,5,9},
  {9,5,2},
  {8,9,3},

  {2,6,9},
  {9,6,7},
  {7,6,0},
  {9,7,3}
};

vtkIdType quadTetEdges[][2] =
{
  {0,4},
  {4,1},
  {1,5},
  {5,2},
  {2,6},
  {6,0},
  {0,7},
  {7,3},
  {1,8},
  {8,3},
  {2,9},
  {9,3}
};

/* Each face should look like this:
 *             +-+-+
 *             |\|/|
 *             +-+-+
 *             |/|\|
 *             +-+-+
 * This tessellation is required for
 * neighboring hexes to have compatible
 * boundaries.
 */
vtkIdType quadHexTetrahedra[][4] =
{
  { 0, 8,20,26},
  { 8, 1,20,26},
  { 1, 9,20,26},
  { 9, 2,20,26},
  { 2,10,20,26},
  {10, 3,20,26},
  { 3,11,20,26},
  {11, 0,20,26},

  { 4,15,21,26},
  {15, 7,21,26},
  { 7,14,21,26},
  {14, 6,21,26},
  { 6,13,21,26},
  {13, 5,21,26},
  { 5,12,21,26},
  {12, 4,21,26},

  { 0,16,22,26},
  {16, 4,22,26},
  { 4,12,22,26},
  {12, 5,22,26},
  { 5,17,22,26},
  {17, 1,22,26},
  { 1, 8,22,26},
  { 8, 0,22,26},

  { 3,10,23,26},
  {10, 2,23,26},
  { 2,18,23,26},
  {18, 6,23,26},
  { 6,14,23,26},
  {14, 7,23,26},
  { 7,19,23,26},
  {19, 3,23,26},

  { 0,11,24,26},
  {11, 3,24,26},
  { 3,19,24,26},
  {19, 7,24,26},
  { 7,15,24,26},
  {15, 4,24,26},
  { 4,16,24,26},
  {16, 0,24,26},

  { 1,17,25,26},
  {17, 5,25,26},
  { 5,13,25,26},
  {13, 6,25,26},
  { 6,18,25,26},
  {18, 2,25,26},
  { 2, 9,25,26},
  { 9, 1,25,26}
};

vtkIdType quadHexTris[][3] =
{
  { 0, 8,20},
  { 8, 1,20},
  { 1, 9,20},
  { 9, 2,20},
  { 2,10,20},
  {10, 3,20},
  { 3,11,20},
  {11, 0,20},

  { 4,15,21},
  {15, 7,21},
  { 7,14,21},
  {14, 6,21},
  { 6,13,21},
  {13, 5,21},
  { 5,12,21},
  {12, 4,21},

  { 0,16,22},
  {16, 4,22},
  { 4,12,22},
  {12, 5,22},
  { 5,17,22},
  {17, 1,22},
  { 1, 8,22},
  { 8, 0,22},

  { 3,10,23},
  {10, 2,23},
  { 2,18,23},
  {18, 6,23},
  { 6,14,23},
  {14, 7,23},
  { 7,19,23},
  {19, 3,23},

  { 0,11,24},
  {11, 3,24},
  { 3,19,24},
  {19, 7,24},
  { 7,15,24},
  {15, 4,24},
  { 4,16,24},
  {16, 0,24},

  { 1,17,25},
  {17, 5,25},
  { 5,13,25},
  {13, 6,25},
  { 6,18,25},
  {18, 2,25},
  { 2, 9,25},
  { 9, 1,25}
};

vtkIdType quadHexEdges[][2] =
{
  { 0, 8},
  { 8, 1},
  { 1, 9},
  { 9, 2},
  { 2,10},
  {10, 3},
  { 3,11},
  {11, 0},
  { 4,15},
  {15, 7},
  { 7,14},
  {14, 6},
  { 6,13},
  {13, 5},
  { 5,12},
  {12, 4},
  { 0,16},
  {16, 4},
  { 5,17},
  {17, 1},
  { 2,18},
  {18, 6},
  { 7,19},
  {19, 3}
};

// ========================================
// the meat of the class: execution!
void vtkTempTessellatorFilter::Execute()
{
  static double weights[27];
  int dummySubId=-1;
  int p;

  vtkDataSet* mesh = this->GetInput();

  this->SetupOutput();

  this->Subdivider->SetMesh( mesh );
  this->Tessellator->SetEdgeCallback( AddALine );
  this->Tessellator->SetTriangleCallback( AddATriangle );
  this->Tessellator->SetTetrahedronCallback( AddATetrahedron );
  this->Tessellator->SetPrivateData( this );

  vtkIdType cell;
  int nprim;
  vtkIdType* outconn;
  double pts[27][11 + vtkStreamingTessellator::MaxFieldSize];
  int c;

  for ( cell = 0; cell < mesh->GetNumberOfCells(); ++cell )
    {
    this->Subdivider->SetCellId( cell );
    vtkCell* cp = this->Subdivider->GetCell(); // We set the cell ID, get the vtkCell pointer
    int np = cp->GetCellType();
    if ( np == VTK_VOXEL || np == VTK_HEXAHEDRON || np == VTK_QUADRATIC_HEXAHEDRON )
      np = 27;
    else
      np = cp->GetNumberOfPoints();
    double* pcoord = cp->GetParametricCoords();
    double* gcoord;
    vtkDataArray* field;
    for ( p = 0; p < cp->GetNumberOfPoints(); ++p )
      {
      gcoord = cp->Points->GetPoint( p );
      for ( c = 0; c < 3; ++c, ++gcoord, ++pcoord )
        {
        pts[p][c  ] = *gcoord;
        pts[p][c+3] = *pcoord;
        }
      // fill in field data
      const int* offsets = this->Subdivider->GetFieldOffsets();
      for ( int f = 0; f < this->Subdivider->GetNumberOfFields(); ++f )
        {
        field = mesh->GetPointData()->GetArray( this->Subdivider->GetFieldIds()[ f ] );
        double* tuple = field->GetTuple( cp->GetPointId( p ) );
        for ( c = 0; c < field->GetNumberOfComponents(); ++c )
          {
          pts[p][ 6 + offsets[f] + c ] = tuple[c];
          }
        }
      }
    int dim = this->OutputDimension;
    // Tessellate each cell:
    switch ( cp->GetCellType() )
      {
    case VTK_LINE:
      dim = 1;
      outconn = &linEdgeEdges[0][0];
      nprim = sizeof(linEdgeEdges)/sizeof(linEdgeEdges[0]);
      break;
    case VTK_POLY_LINE:
      dim = -1;
      vtkWarningMacro( "Oops, POLY_LINE not supported" );
      break;
    case VTK_TRIANGLE:
      if ( dim > 1 )
        {
        dim = 2;
        outconn = &linTriTris[0][0];
        nprim = sizeof(linTriTris)/sizeof(linTriTris[0]);
        }
      else
        {
        outconn = &linTriEdges[0][0];
        nprim = sizeof(linTriEdges)/sizeof(linTriEdges[0]);
        }
      break;
    case VTK_TRIANGLE_STRIP:
      dim = -1;
      vtkWarningMacro( "Oops, TRIANGLE_STRIP not supported" );
      break;
    case VTK_POLYGON:
      dim = -1;
      vtkWarningMacro( "Oops, POLYGON not supported" );
      break;
    case VTK_QUAD:
      if ( dim > 1 )
        {
        dim = 2;
        outconn = &linQuadTris[0][0];
        nprim = sizeof(linQuadTris)/sizeof(linQuadTris[0]);
        }
      else
        {
        outconn = &linQuadEdges[0][0];
        nprim = sizeof(linQuadEdges)/sizeof(linQuadEdges[0]);
        }
      break;
    case VTK_TETRA:
      if ( dim == 3 )
        {
        outconn = &linTetTetrahedra[0][0];
        nprim = sizeof(linTetTetrahedra)/sizeof(linTetTetrahedra[0]);
        }
      else if ( dim == 2 )
        {
        outconn = &linTetTris[0][0];
        nprim = sizeof(linTetTris)/sizeof(linTetTris[0]);
        }
      else
        {
        outconn = &linTetEdges[0][0];
        nprim = sizeof(linTetEdges)/sizeof(linTetEdges[0]);
        }
      break;
    case VTK_WEDGE:
      if ( dim == 3 )
        {
        outconn = &linWedgeTetrahedra[0][0];
        nprim = sizeof(linWedgeTetrahedra)/sizeof(linWedgeTetrahedra[0]);
        }
      else if ( dim ==2 )
        {
        outconn = &linWedgeTris[0][0];
        nprim = sizeof(linWedgeTris)/sizeof(linWedgeTris[0]);
        }
      else
        {
        outconn = &linWedgeEdges[0][0];
        nprim = sizeof(linWedgeEdges)/sizeof(linWedgeEdges[0]);
        }
      break;
    case VTK_PYRAMID:
      if ( dim == 3 )
        {
        outconn = &linPyrTetrahedra[0][0];
        nprim = sizeof(linPyrTetrahedra)/sizeof(linPyrTetrahedra[0]);
        }
      else if ( dim ==2 )
        {
        outconn = &linPyrTris[0][0];
        nprim = sizeof(linPyrTris)/sizeof(linPyrTris[0]);
        }
      else
        {
        outconn = &linPyrEdges[0][0];
        nprim = sizeof(linPyrEdges)/sizeof(linPyrEdges[0]);
        }
      break;
    case VTK_QUADRATIC_EDGE:
      dim = 1;
      outconn = &quadEdgeEdges[0][0];
      nprim = sizeof(quadEdgeEdges)/sizeof(quadEdgeEdges[0]);
      break;
    case VTK_QUADRATIC_TRIANGLE:
      if ( dim > 1 )
        {
        dim = 2;
        outconn = &quadTriTris[0][0];
        nprim = sizeof(quadTriTris)/sizeof(quadTriTris[0]);
        }
      else
        {
        outconn = &quadTriEdges[0][0];
        nprim = sizeof(quadTriEdges)/sizeof(quadTriEdges[0]);
        }
      break;
    case VTK_QUADRATIC_QUAD:
      for ( c = 0; c < 3; ++c )
        pts[8][c+3] = extraQuadQuadParams[0][c];
      cp->EvaluateLocation( dummySubId, pts[8] + 3, pts[8], weights );
      this->Subdivider->EvaluateFields( pts[8], weights, 6 );
      if ( dim > 1 )
        {
        dim = 2;
        outconn = &quadQuadTris[0][0];
        nprim = sizeof(quadQuadTris)/sizeof(quadQuadTris[0]);
        }
      else
        {
        outconn = &quadQuadEdges[0][0];
        nprim = sizeof(quadQuadEdges)/sizeof(quadQuadEdges[0]);
        }
      break;
    case VTK_QUADRATIC_TETRA:
      if ( dim == 3 )
        {
        outconn = &quadTetTetrahedra[0][0];
        nprim = sizeof(quadTetTetrahedra)/sizeof(quadTetTetrahedra[0]);
        }
      else if ( dim ==2 )
        {
        outconn = &quadTetTris[0][0];
        nprim = sizeof(quadTetTris)/sizeof(quadTetTris[0]);
        }
      else
        {
        outconn = &quadTetEdges[0][0];
        nprim = sizeof(quadTetEdges)/sizeof(quadTetEdges[0]);
        }
      break;
    case VTK_HEXAHEDRON:
      // we sample 19 extra points to guarantee a compatible tetrahedralization
      for ( p = 8; p < 20; ++p )
        {
        int dummySubId=-1;
        for ( int y = 0; y < 3; ++y )
          pts[p][y+3] = extraLinHexParams[p-8][y];
        cp->EvaluateLocation( dummySubId, pts[p] + 3, pts[p], weights );
        this->Subdivider->EvaluateFields( pts[p], weights, 6 );
        }
      // fall through
    case VTK_QUADRATIC_HEXAHEDRON:
      for ( p = 20; p < 27; ++p )
        {
        int dummySubId=-1;
        for ( int x = 0; x < 3; ++x )
          pts[p][x+3] = extraQuadHexParams[p-20][x];
        cp->EvaluateLocation( dummySubId, pts[p] + 3, pts[p], weights );
        this->Subdivider->EvaluateFields( pts[p], weights, 6 );
        }
      if ( dim == 3 )
        {
        outconn = &quadHexTetrahedra[0][0];
        nprim = sizeof(quadHexTetrahedra)/sizeof(quadHexTetrahedra[0]);
        }
      else if ( dim ==2 )
        {
        outconn = &quadHexTris[0][0];
        nprim = sizeof(quadHexTris)/sizeof(quadHexTris[0]);
        }
      else
        {
        outconn = &quadHexEdges[0][0];
        nprim = sizeof(quadHexEdges)/sizeof(quadHexEdges[0]);
        }
      break;
    case VTK_VOXEL:
    case VTK_PIXEL:
      dim = -1;
      vtkWarningMacro( "Oops, voxels and pixels not supported" );
      break;
    default:
      dim = -1;
      vtkWarningMacro( "Oops, something not supported" );
      }

    // OK, now output the primitives
    int tet, tri, edg;
    switch ( dim )
      {
    case 3:
      for ( tet=0; tet<nprim; ++tet, outconn += 4 )
        this->Tessellator->AdaptivelySample3Facet( pts[outconn[0]], pts[outconn[1]], pts[outconn[2]], pts[outconn[3]] );
      break;
    case 2:
      for ( tri=0; tri<nprim; ++tri, outconn += 3 )
        this->Tessellator->AdaptivelySample2Facet( pts[outconn[0]], pts[outconn[1]], pts[outconn[2]] );
      break;
    case 1:
      for ( edg=0; edg<nprim; ++edg, outconn += 2 )
        this->Tessellator->AdaptivelySample1Facet( pts[outconn[0]], pts[outconn[1]] );
      break;
    default:
      // do nothing
      break;
      }
    }

  this->Teardown();
}

