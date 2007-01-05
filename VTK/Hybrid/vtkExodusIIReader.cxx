#include "vtkExodusIIReader.h"
#include "vtkExodusIICache.h"

#include "vtkCellData.h"
#include "vtkCellType.h"
#include "vtkDataArray.h"
#include "vtkDoubleArray.h"
#include "vtkDSPFilterDefinition.h"
#include "vtkExodusModel.h"
#include "vtkFloatArray.h"
#include "vtkIdTypeArray.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkIntArray.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkSortDataArray.h"
#include "vtkStdString.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkUnstructuredGrid.h"
#include "vtkXMLParser.h"

#include <vtkstd/vector>
#include <vtkstd/map>
#include "vtksys/SystemTools.hxx"

#include "vtksys/RegularExpression.hxx"

#include "exodusII.h"
#include <stdio.h>
#include <stdlib.h> /* for free() */
#include <string.h> /* for memset() */
#include <ctype.h> /* for toupper(), isgraph() */

#ifdef EXODUSII_HAVE_MALLOC_H
#  include <malloc.h>
#endif /* EXODUSII_HAVE_MALLOC_H */

/// Define this to get printouts summarizing array glomming process
#undef VTK_DBG_GLOM

#define VTK_EXO_BLKSETID_NAME  "BlockId"

#define VTK_EXO_FUNC(funcall,errmsg)\
  if ( (funcall) < 0 ) \
    { \
      vtkErrorMacro( errmsg ); \
      return 1; \
    }

// ------------------------------------------------------------------- CONSTANTS
static int obj_types[] = {
  EX_EDGE_BLOCK,
  EX_FACE_BLOCK,
  EX_ELEM_BLOCK,
  EX_NODE_SET,
  EX_EDGE_SET,
  EX_FACE_SET,
  EX_SIDE_SET,
  EX_ELEM_SET,
  EX_NODE_MAP,
  EX_EDGE_MAP,
  EX_FACE_MAP,
  EX_ELEM_MAP
};

static int num_obj_types = (int)(sizeof(obj_types)/sizeof(obj_types[0]));

static int obj_sizes[] = {
  EX_INQ_EDGE_BLK,
  EX_INQ_FACE_BLK,
  EX_INQ_ELEM_BLK,
  EX_INQ_NODE_SETS,
  EX_INQ_EDGE_SETS,
  EX_INQ_FACE_SETS,
  EX_INQ_SIDE_SETS,
  EX_INQ_ELEM_SETS,
  EX_INQ_NODE_MAP,
  EX_INQ_EDGE_MAP,
  EX_INQ_FACE_MAP,
  EX_INQ_ELEM_MAP,
};

static const char* objtype_names[] = {
  "Edge block",
  "Face block",
  "Element block",
  "Node set",
  "Edge set",
  "Face set",
  "Side set",
  "Element set",
  "Node map",
  "Edge map",
  "Face map",
  "Element map"
};

static const char* obj_typestr[] = {
  "L",
  "F",
  "E",
  "M",
  "D",
  "A",
  "S",
  "T",
  0, /* maps have no result variables */
  0,
  0,
  0,
};

#define OBJTYPE_IS_BLOCK(i) ((i>=0)&&(i<3))
#define OBJTYPE_IS_SET(i) ((i>2)&&(i<8))
#define OBJTYPE_IS_MAP(i) ((i>7)&&(i<12))

// Unlike obj* items above:
// - conn* arrays only reference objects that generate connectivity information
// - conn* arrays are ordered the way users expect the output (*not* the same as above)
static int conn_types[] = {
  vtkExodusIIReader::ELEM_BLOCK_ELEM_CONN,
  vtkExodusIIReader::FACE_BLOCK_CONN,
  vtkExodusIIReader::EDGE_BLOCK_CONN,
  vtkExodusIIReader::ELEM_SET_CONN,
  vtkExodusIIReader::SIDE_SET_CONN,
  vtkExodusIIReader::FACE_SET_CONN,
  vtkExodusIIReader::EDGE_SET_CONN,
  vtkExodusIIReader::NODE_SET_CONN
};

static int num_conn_types = (int)(sizeof(conn_types)/sizeof(conn_types[0]));

// Given a conn_type index, what is its matching obj_type index?
static int conn_obj_idx_cvt[] = {
  2, 1, 0, 7, 6, 5, 4, 3
};

#define CONNTYPE_IS_BLOCK(i) ((i>=0)&&(i<3))
#define CONNTYPE_IS_SET(i) ((i>2)&&(i<8))

static const char* glomTypeNames[] = {
  "Scalar",
  "Vector2",
  "Vector3",
  "Symmetric Tensor",
  "Integration Point Values"
};

// used to store pointer to ex_get_node_num_map or ex_get_elem_num_map:
extern "C" { typedef int (*vtkExodusIIGetMapFunc)( int, int* ); }

// --------------------------------------------------- PRIVATE CLASS DECLARATION

/** This class holds metadata for an Exodus file.
 *
 */
class vtkExodusIIReaderPrivate : public vtkObject
{
public:
  static vtkExodusIIReaderPrivate* New();
  void PrintData( ostream& os, vtkIndent indent );
  vtkTypeRevisionMacro(vtkExodusIIReaderPrivate,vtkObject);

  /// Open an ExodusII file for reading. Returns 0 on success.
  int OpenFile( const char* filename );

  /// Close any ExodusII file currently open for reading. Returns 0 on success.
  int CloseFile();

  /// Get metadata for an open file with handle \a exoid.
  int RequestInformation();

  /// Read requested data and store in unstructured grid.
  int RequestData( vtkIdType timeStep, vtkUnstructuredGrid* output );

  /// Reset the class so that another file may be read.
  void Reset();

  /** Return the number of time steps in the open file.
    * You must have called RequestInformation() before invoking this member function.
    */
  int GetNumberOfTimeSteps() { return (int) this->Times.size(); }

  /// Return the current time step
  vtkGetMacro(TimeStep,int);

  /// Set the current time step for subsequent calls to RequestData().
  vtkSetMacro(TimeStep,int);

  /// Return whether subsequent RequestData() calls will produce the minimal point set required to represent the output.
  vtkGetMacro(SqueezePoints,int);

  /// Set whether subsequent RequestData() calls will produce the minimal point set required to represent the output.
  void SetSqueezePoints( int sp );

  /// Convenience routines that for producing (or not) the minimal point set required to represent the output.
  vtkBooleanMacro(SqueezePoints,int);

  /// Return the number of nodes in the output (depends on SqueezePoints)
  int GetNumberOfNodes();

  /** Returns the number of objects of a given type (e.g., EX_ELEM_BLOCK, EX_NODE_SET, ...).
    * You must have called RequestInformation before invoking this member function.
    */
  int GetNumberOfObjectsOfType( int otype );

  /** Returns the number of arrays defined over objects of a given type (e.g., EX_ELEM_BLOCK, EX_NODE_SET, ...).
    * You must have called RequestInformation before invoking this member function.
    *
    * N.B.: This method will eventually disappear. Really, what we should be providing is an interface to
    * query the arrays defined on a particular object, not a class of objects. However, until the reader
    * outputs multiblock datasets, we can't be that specific.
    */
  int GetNumberOfObjectArraysOfType( int otype );

  /** For a given object type, returns the name of the i-th object. 
    * You must have called RequestInformation before invoking this member function.
    */
  const char* GetObjectName( int otype, int i );

  /** For a given object type, return the user-assigned ID of the i-th object.
    * You must have called RequestInformation before invoking this member function.
    */
  int GetObjectId( int otype, int i );

  /** For a given object type, return the size of the i-th object.
    * The size is the number of entries.
    * As an example, for an element block, it is the number of elements.
    * You must have called RequestInformation before invoking this member function.
    */
  int GetObjectSize( int otype, int i );

  /** For a given object type, returns the status of the i-th object.
    * You must have called RequestInformation before invoking this member function.
    */
  int GetObjectStatus( int otype, int i );

  /** For a given object type, sets the status of the i-th object.
    * You must have called RequestInformation before invoking this member function.
    */
  void SetObjectStatus( int otype, int i, int stat );

  /** For a given object type, returns the name of the i-th array. 
    * You must have called RequestInformation before invoking this member function.
    */
  const char* GetObjectArrayName( int otype, int i );

  /** For a given object type, returns the number of components of the i-th array.
    * You must have called RequestInformation before invoking this member function.
    */
  int GetNumberOfObjectArrayComponents( int otype, int i );

  /** For a given object type, returns the status of the i-th array.
    * You must have called RequestInformation before invoking this member function.
    */
  int GetObjectArrayStatus( int otype, int i );

  /** For a given object type, sets the status of the i-th array.
    * You must have called RequestInformation before invoking this member function.
    */
  void SetObjectArrayStatus( int otype, int i, int stat );

  int GetNumberOfObjectAttributes( int objectType, int objectIndex );
  const char* GetObjectAttributeName( int objectType, int objectIndex, int attributeIndex );
  int GetObjectAttributeIndex( int objectType, int objectIndex, const char* attribName );
  int GetObjectAttributeStatus( int objectType, int objectIndex, int attribIndex );
  void SetObjectAttributeStatus( int objectType, int objectIndex, int attribIndex, int status );

  /// Generate an array containing the block or set ID associated with each cell.
  vtkGetMacro(GenerateObjectIdArray,int);
  vtkSetMacro(GenerateObjectIdArray,int);
  const char* GetObjectIdArrayName() { return "ObjectId"; }

  vtkSetMacro(GenerateGlobalElementIdArray,int);
  vtkGetMacro(GenerateGlobalElementIdArray,int);
  static const char *GetGlobalElementIdArrayName() { return "GlobalElementId"; }  

  vtkSetMacro(GenerateGlobalNodeIdArray,int);
  vtkGetMacro(GenerateGlobalNodeIdArray,int);
  static const char *GetGlobalNodeIdArrayName() { return "GlobalNodeId"; }  

  virtual void SetApplyDisplacements( int d );
  vtkGetMacro(ApplyDisplacements,int);

  virtual void SetDisplacementMagnitude( double s );
  vtkGetMacro(DisplacementMagnitude,double);

  vtkDataArray* FindDisplacementVectors( int timeStep );

  const struct ex_init_params* GetModelParams() const { return &this->ModelParameters; }

  /// A struct to hold information about time-varying arrays
  struct ArrayInfoType {
    /// The name of the array
    vtkStdString Name;
    /// The number of components in the array
    int Components;
    /** The type of "glomming" performed.
     * Glomming is the process of aggregating one or more results variable names
     * from the Exodus files into a single VTK result variable name with one or more
     * components.
     * One of: scalar, vector(2), vector(3), symtensor(6), integrationpoint.
     */
    int GlomType;
    /// Storage type of array (a type that can be passed to vtkDataArray::Create())
    int StorageType;
    /// The source of the array (Result or Attribute)
    int Source;
    /// Whether or not the array should be loaded by RequestData
    int Status;
    /// The name of each component of the array as defined by the Exodus file. Empty for generated arrays.
    vtkstd::vector<vtkStdString> OriginalNames;
    /// The index of each component of the array as ordered by the Exodus file. Empty for generated arrays.
    vtkstd::vector<int> OriginalIndices;
    /** A map describing which objects the variable is defined on.
     * Each key (a pair<int,int>) is a block/set type and integer
     * offset into the corresponding BlockInfo or SetInfo.
     * Its value is true when the variable is defined on the
     * block/set indicated by the key.
     * Otherwise (if the key is absent from the map or present with a
     * false value), the variable is not defined on that block/set.
     */
    vtkstd::vector<int> ObjectTruth;
    /// Clear all the structure members.
    void Reset();
  };

  /// A struct to hold information about Exodus objects (blocks, sets, maps)
  struct ObjectInfoType {
    /// Number of entries in this block.
    int Size;
    /// Should the reader load this block?
    int Status;
    /// User-assigned identification number
    int Id;
    /// User-assigned name
    vtkStdString Name;
  };

  /// A struct to hold information about Exodus maps
  struct MapInfoType : public ObjectInfoType {
  };

  /// A struct to hold information about Exodus blocks or sets (they have some members in common)
  struct BlockSetInfoType : public ObjectInfoType {
    /// Id (1-based) of first entry in file-local list across all blocks in file
    vtkIdType FileOffset;
    /// Id (0-based) of first entry in the vtkUnstructuredGrid containing all blocks with Status != 0
    vtkIdType GridOffset;
  };

  /// A struct to hold information about Exodus blocks
  struct BlockInfoType : public BlockSetInfoType {
    vtkStdString TypeName;
    int BdsPerEntry[3]; // number of boundaries per entry. The index is the dimensionality of the entry. 0=node, 1=edge, 2=face
    int AttributesPerEntry;
    vtkstd::vector<vtkStdString> AttributeNames;
    vtkstd::vector<int> AttributeStatus;
    int CellType; // VTK cell type (a function of TypeName and BdsPerEntry...)
    int PointsPerCell; // Number of points per cell as used by VTK -- not what's in the file (i.e., BdsPerEntry[0] >= PointsPerCell)
  };

  /// A struct to hold information about Exodus sets
  struct SetInfoType : public BlockSetInfoType {
    int DistFact; // Number of distribution factors (for the entire block, not per array or entry)
  };

  /// Tags to indicate how single-component Exodus arrays are glommed (aggregated) into multi-component VTK arrays.
  enum GlomTypes {
    Scalar=0,          //!< The array is a scalar
    Vector2=1,         //!< The array is a 2-D vector
    Vector3=2,         //!< The array is a 3-D vector
    SymmetricTensor=3, //!< The array is a symmetric tensor (order xx, yy, zz, xy, yz, zx)
    IntegrationPoint=4 //!< The array is a set of integration point values
  };

  /// Tags to indicate the source of values for an array.
  enum ArraySourceTypes {
    Result=0,        //!< The array is composed of results variables (that vary over time)
    Attribute=1,     //!< The array is composed of attributes (constants over time)
    Map=2,           //!< The array has a corresponding entry in MapInfo
    Generated=3      //!< The array is procedurally generated (e.g., BlockId)
  };

  friend class vtkExodusIIReader;

protected:
  vtkExodusIIReaderPrivate();
  ~vtkExodusIIReaderPrivate();

  /// Any time the Status member of a block or set changes, this function must be called.
  void ComputeGridOffsets();

  /// Returns true when order and text of names are consistent with integration points. Called from GlomArrayNames().
  int VerifyIntegrationPointGlom( int nn, char** np, vtksys::RegularExpression& re, vtkStdString& field, vtkStdString& ele );

  /// Aggregate Exodus array names into VTK arrays with multiple components
  void GlomArrayNames( int i, int num_obj, int num_vars, char** var_names, int* truth_tab );

  /// Add generated array information to array info lists.
  void PrepareGeneratedArrayInfo();

  /** Read connectivity information and populate an unstructured grid with cells.
    * If the connectivity hasn't changed since the last time RequestData was called,
    * this copies a cache to the output.
    * 
    * Otherwise, this routine iterates over all block and set types.
    * For each type, it iterates over all objects of that type.
    * For each object whose status is 1, it reads that object's connectivity entries from
    * cache or disk and inserts cells into CachedConnectivity.
    * If SqueezePoints is on, then connectivity entries are translated as required and
    * PointMap is populated.
    * Finally, CachedConnectivity is shallow-copied to the output.
    * 
    * AssembleOutputConnectivity returns 1 if cache was used, 0 otherwise.
    */
  int AssembleOutputConnectivity( vtkIdType timeStep, vtkUnstructuredGrid* output );

  /** Fill the output grid's point coordinates array.
    * Returns 1 on success, 0 on failure.
    * Failure occurs when the Exodus library is unable to read the point
    * coordindates array. This can be caused when there is not enough memory
    * or there is a file I/O problem.
    */
  int AssembleOutputPoints( vtkIdType timeStep, vtkUnstructuredGrid* output );
  /** Add the requested arrays to the output grid's point data.
    * This adds time-varying results arrays to the grid's vtkPointData object.
    */
  int AssembleOutputPointArrays( vtkIdType timeStep, vtkUnstructuredGrid* output );
  /** Add the requested arrays to the output grid's cell data.
    * This adds time-varying results arrays to the grid's vtkCellData object.
    * Each array added may not be defined on all blocks of cells, so zero-padding will be used where required.
    */
  int AssembleOutputCellArrays( vtkIdType timeStep, vtkUnstructuredGrid* output );
  /** Add maps to an output mesh.
    * Maps are special integer arrays that may serve as GlobalId fields in vtkDataSetAttributes objects.
    * Maps will only be zero-padded when cells representing set entries exist;
    * also, maps may be procedurally generated if no map is contained in a file.
    * Maps are not time-varying.
    */
  int AssembleOutputPointMaps( vtkIdType timeStep, vtkUnstructuredGrid* output );
  int AssembleOutputCellMaps( vtkIdType timeStep, vtkUnstructuredGrid* output );
  /** Add procedurally generated arrays to an output mesh.
    * Currently, the only array that is procedurally generated is the object id array.
    * Others may be added in the future.
    */
  int AssembleOutputProceduralArrays( vtkIdType timeStep, vtkUnstructuredGrid* output );

  /// Insert cells from a specified block into a mesh
  void InsertBlockCells( int otyp, int obj, int conn_type, int timeStep, vtkUnstructuredGrid* output );

  /// Insert cells from a specified set into a mesh
  void InsertSetCells( int otyp, int obj, int conn_type, int timeStep, vtkUnstructuredGrid* output );

  /// Add a point array to an output grid's point data, squeezing if necessary
  void AddPointArray( vtkDataArray* src, vtkUnstructuredGrid* output );

  /// Insert cells referenced by a node set.
  void InsertSetNodeCopies( vtkIntArray* refs, int otyp, int obj, vtkUnstructuredGrid* output );

  /// Insert cells referenced by an edge, face, or element set.
  void InsertSetCellCopies( vtkIntArray* refs, int otyp, int obj, vtkUnstructuredGrid* output );

  /// Insert cells referenced by a side set.
  void InsertSetSides( vtkIntArray* refs, int otyp, int obj, vtkUnstructuredGrid* output );

  /** Return an array for the specified cache key. If the array was not cached, read it from the file.
    * This function can still return 0 if you are foolish enough to request an array not present in the file, grasshopper.
    */
  vtkDataArray* GetCacheOrRead( vtkExodusIICacheKey );

  /** Return the index of an object type (in a private list of all object types).
    * This returns a 0-based index if the object type was found and -1 if it was not.
    */
  int GetConnTypeIndexFromConnType( int ctyp );

  /** Return the index of an object type (in a private list of all object types).
    * This returns a 0-based index if the object type was found and -1 if it was not.
    */
  int GetObjectTypeIndexFromObjectType( int otyp );

  /** Return the number of objects of the given type.
    * The integer typeIndex is not the type of the object (e.g., EX_ELEM_BLOCK), but
    * is rather the index into the list of all object types (see obj_types in vtkExodusIIReader.cxx).
    */
  int GetNumberOfObjectsAtTypeIndex( int typeIndex );

  /** Return a pointer to the ObjectInfo of the specified type and index.
    * The integer typeIndex is not the type of the object (e.g., EX_ELEM_BLOCK), but
    * is rather the index into the list of all object types (see obj_types in vtkExodusIIReader.cxx).
    * The integer objectIndex is not the ID of the object (i.e., the ID stored in the Exodus file),
    * but is rather the index into the list of all objects of the given type.
    */
  ObjectInfoType* GetObjectInfo( int typeIndex, int objectIndex );

  /** Return a pointer to the ObjectInfo of the specified type and index, but using indices sorted by object ID.
    * This is the same as GetObjectInfo() except that it uses the SortedObjectIndices member to permute
    * the requested \a objectIndex and it takes an object type (e.g., EX_ELEM_BLOCK) rather than an object type index.
    */
  ObjectInfoType* GetSortedObjectInfo( int objectType, int objectIndex );

  /** Get the index of the block containing the entity referenced by the specified file-global ID.
    * In this case, an entity is an edge, face, or element.
    */
  int GetBlockIndexFromFileGlobalId( int otyp, int refId );

  /** Get the block containing the entity referenced by the specified file-global ID.
    * In this case, an entity is an edge, face, or element.
    */
  BlockInfoType* GetBlockFromFileGlobalId( int otyp, int refId );

  /// Find or create a new SqueezePoint ID (unique sequential list of points referenced by cells in blocks/sets with Status == 1)
  vtkIdType GetSqueezePointId( int i );

  /// Determine the VTK cell type for a given edge/face/element block
  void DetermineVtkCellType( BlockInfoType& binfo );

  /// Find an ArrayInfo object for a specific object type using the name as a key.
  ArrayInfoType* FindArrayInfoByName( int otyp, const char* name );

  /// Does the specified object type match? Avoid using these... they aren't robust against new types being implemented.
  int IsObjectTypeBlock( int otyp );
  int IsObjectTypeSet( int otyp );
  int IsObjectTypeMap( int otyp );

  /// Given a map type (NODE_MAP, EDGE_MAP, ...) return the associated object type (NODAL, EDGE_BLOCK, ...) or vice-versa.
  int GetObjectTypeFromMapType( int mtyp );
  int GetMapTypeFromObjectType( int otyp );

  /// Given a set connectivity type (NODE_SET_CONN, ...), return the associated object type (NODE_SET, ...) or vice-versa.
  int GetSetTypeFromSetConnType( int sctyp );

  /// Given a block type (EDGE_BLOCK, ...), return the associated block connectivity type (EDGE_BLOCK_CONN, ...) or vice-versa.
  int GetBlockConnTypeFromBlockType( int btyp );

  /// Get/Set the cached connectivity data
  vtkGetObjectMacro(CachedConnectivity,vtkUnstructuredGrid);
  virtual void SetCachedConnectivity( vtkUnstructuredGrid* mesh );

  /** Function to trim space from names retrieved with ex_get_var_names.
   * This was added because some meshes had displacement arrays named "DISPX ", "DISPY ", "DISPZ " (note trailing spaces),
   * which prevented glomming and use of the vector field for displacements.
   */
  void RemoveBeginningAndTrailingSpaces( int len, char **names );

  // The next vtk ID to use for a connectivity entry when point squeezing is on and no point ID exists.
  vtkIdType NextSqueezePoint;

  /// Maps a block type (EX_ELEM_BLOCK, EX_FACE_BLOCK, ...) to a list of blocks of that type.
  vtkstd::map<int,vtkstd::vector<BlockInfoType> > BlockInfo;
  /// Maps a set type (EX_ELEM_SET, ..., EX_NODE_SET) to a list of sets of that type.
  vtkstd::map<int,vtkstd::vector<SetInfoType> > SetInfo;
  /** Maps a map type (EX_ELEM_MAP, ..., EX_NODE_MAP) to a list of maps of that type.
    * In old-style files, the only entries will be a single node and a single element map
    * which have no specified ID number or name. In that case, an ID of 0 and a name of
    * "Default" will be given to both.
    */
  vtkstd::map<int,vtkstd::vector<MapInfoType> > MapInfo;
  /** Maps an object type to vector of indices that reorder objects of that type by their IDs.
    * This is used by the user interface to access blocks, sets, and maps in ascending order.
    * It is not used internally.
    */
  vtkstd::map<int,vtkstd::vector<int> > SortedObjectIndices;
  /// Maps an object type (EX_ELEM_BLOCK, EX_NODE_SET, ...) to a list of arrays defined on that type.
  vtkstd::map<int,vtkstd::vector<ArrayInfoType> > ArrayInfo;

  /// These aren't the variables you're looking for.
  int AppWordSize;
  int DiskWordSize;

  /// The version of Exodus that wrote the currently open file (or a negative number otherwise).
  float ExodusVersion;

  /// The handle of the currently open file.
  int Exoid;

  /// Parameters describing the currently open Exodus file.
  struct ex_init_params ModelParameters;

  /// A list of time steps for which results variables are stored.
  vtkstd::vector<double> Times;

  /// The current time step
  int TimeStep;

  int GenerateObjectIdArray;
  int GenerateGlobalIdArray;

  /// A least-recently-used cache to hold raw arrays.
  vtkExodusIICache* Cache;

  /// Cache assembled connectivity separately because there's no way to SetLinks() on a vtkUnstructuredGrid.
  vtkUnstructuredGrid* CachedConnectivity;

  int GenerateGlobalElementIdArray;
  int GenerateGlobalNodeIdArray;
  int ApplyDisplacements;
  float DisplacementMagnitude;

  /** Should the reader output only points used by elements in the output mesh, or all the points.
    * Outputting all the points is much faster since the point array can be read straight from
    * disk and the mesh connectivity need not be altered.
    * Squeezing the points down to the minimum set needed to produce the output mesh is useful for
    * glyphing and other point-based operations. On large parallel datasets, loading all the points
    * implies loading all the points on all processes and performing subsequent filtering on a much
    * larger set.
    *
    * By default, SqueezePoints is true for backwards compatability.
    */
  int SqueezePoints;

  /// The total number of cells in the mesh given the current block and set Status values.
  vtkIdType NumberOfCells;

  /// The total number of points in the mesh given the SqueezePoints setting (and possibly the block and set Status values).
  //vtkIdType NumberOfPoints;

  /// A map from nodal IDs in an Exodus file to nodal IDs in the output mesh.
  vtkstd::vector<vtkIdType> PointMap;

  /// Pointer to owning reader... this is not registered in order to avoid circular references.
  vtkExodusIIReader* Parent;

private:
  vtkExodusIIReaderPrivate( const vtkExodusIIReaderPrivate& ); // Not implemented.
  void operator = ( const vtkExodusIIReaderPrivate& ); // Not implemented.
};

// ------------------------------------------------------------ UTILITY ROUTINES
static int glomIntegrationPointElementDimension( vtkStdString& eleType )
{
  vtksys::RegularExpression reQuad( "[Qq][Uu][Aa][Dd]" );
  vtksys::RegularExpression reHex( "[Hh][Ee][Xx]" );
  vtksys::RegularExpression reTet( "[Tt][Ee][Tt]" );
  vtksys::RegularExpression reTri( "[Tt][Rr][Ii]" );
  vtksys::RegularExpression reWedge( "[Ww][Ee][Dd][Gg][Ee]" );
  vtksys::RegularExpression rePyramid( "[Pp][Yy][Rr]" );
  if ( reHex.find( eleType ) )
    return 3;
  else if ( reTet.find( eleType ) )
    return 3;
  else if ( reWedge.find( eleType ) )
    return 3;
  else if ( rePyramid.find( eleType ) )
    return 3;
  else if ( reQuad.find( eleType ) )
    return 2;
  else if ( reTri.find( eleType ) )
    return 2;
  
  return -1;
}

static int glomTruthTabMatch( int num_obj, int num_vars, int* truth_tab, vtkExodusIIReaderPrivate::ArrayInfoType& ainfo )
{
  // This returns 1 when all objects have the same values
  // in truth_tab for all original variable indices in
  // ainfo (and 0 otherwise).
  // It creates an entry in ainfo.ObjectTruth for each object
  // based on the values in truth_tab.
  int num_comp = ainfo.OriginalIndices.size();
  if ( num_comp < 1 )
    return 0;

  int obj;
  int ttObj; // truth table entry for variable idx on object obj.
  int idx = ainfo.OriginalIndices[0] - 1;
  for ( obj = 0; obj < num_obj; ++obj )
    {
    ttObj = truth_tab[ idx + obj * num_vars ];
    ainfo.ObjectTruth.push_back( ttObj );
    }
  if ( num_comp < 2 )
    return 1;

  int comp;
  for ( comp = 1; comp < num_comp; ++comp )
    {
    // Get truth table entry for 0-th variable of object obj:
    for ( obj = 0; obj < num_obj; ++obj )
      {
      if ( truth_tab[ ainfo.OriginalIndices[comp] - 1 + obj * num_vars ] != 
           truth_tab[ idx                             + obj * num_vars ] )
        {
        // At least one object has a different truth table entry for variable ii.
        return 0;
        }
      }
    }
  return 1; // All objects define variable ii over the same subset of objects.
}

static void printBlock( ostream& os, vtkIndent indent, int btyp, vtkExodusIIReaderPrivate::BlockInfoType& binfo )
{
  int b = 0;
  while ( obj_types[b] >= 0 && obj_types[b] != btyp )
    ++b;
  const char* btypnam = objtype_names[b];
  os << indent << btypnam << " " << binfo.Id << " \"" << binfo.Name.c_str() << "\" (" << binfo.Size << ")\n";
  os << indent << "    FileOffset: " << binfo.FileOffset << "\n";
  os << indent << "    GridOffset: " << binfo.GridOffset << " (" << binfo.Status << ")\n";
  os << indent << "    Type: " << binfo.TypeName.c_str() << "\n";
  os << indent << "    Bounds per entry, Node: " << binfo.BdsPerEntry[0]
     << " Edge: " << binfo.BdsPerEntry[1] << " Face: " << binfo.BdsPerEntry[2] << "\n";
  os << indent << "    Attributes (" << binfo.AttributesPerEntry << "):";
  int a;
  for ( a = 0; a < binfo.AttributesPerEntry; ++a )
    {
    os << " \"" << binfo.AttributeNames[a].c_str() << "\"(" << binfo.AttributeStatus[a] << ")";
    }
  os << "\n";
}

static void printSet( ostream& os, vtkIndent indent, int styp, vtkExodusIIReaderPrivate::SetInfoType& sinfo )
{
  int s = 0;
  while ( obj_types[s] >= 0 && obj_types[s] != styp )
    ++s;
  const char* stypnam = objtype_names[s];
  os << indent << stypnam << " " << sinfo.Id << " \"" << sinfo.Name.c_str() << "\" (" << sinfo.Size << ")\n";
  os << indent << "    FileOffset: " << sinfo.FileOffset << "\n";
  os << indent << "    GridOffset: " << sinfo.GridOffset << " (" << sinfo.Status << ")\n";
  os << indent << "    DistFact: " << sinfo.DistFact << "\n";
}

static void printMap( ostream& os, vtkIndent indent, int mtyp, vtkExodusIIReaderPrivate::MapInfoType& minfo )
{
  int m = 0;
  while ( obj_types[m] >= 0 && obj_types[m] != mtyp )
    ++m;
  const char* mtypnam = objtype_names[m];
  os << indent << mtypnam << " " << minfo.Id << " \"" << minfo.Name.c_str() << "\" (" << minfo.Size << ")\n";
  os << indent << "    Status: " << minfo.Status << "\n";
}

static void printArray( ostream& os, vtkIndent indent, int atyp, vtkExodusIIReaderPrivate::ArrayInfoType& ainfo )
{
  (void)atyp;
  os << indent << "    " << ainfo.Name.c_str() << " [" << ainfo.Status << "] ( " << ainfo.Components << " = { ";
  os << ainfo.OriginalIndices[0] << " \"" << ainfo.OriginalNames[0] << "\"";
  int i;
  for ( i = 1; i < (int) ainfo.OriginalIndices.size(); ++i )
    {
    os << ", " << ainfo.OriginalIndices[i] << " \"" << ainfo.OriginalNames[i] << "\"";
    }
  os << " } )\n";
  os << indent << "    " << glomTypeNames[ ainfo.GlomType ] << " Truth:";
  for ( i = 0; i < (int)ainfo.ObjectTruth.size(); ++i )
    {
    os << " " << ainfo.ObjectTruth[i];
    }
  os << "\n";
}

// ---------------------------------------------------- PRIVATE SUBCLASS MEMBERS
void vtkExodusIIReaderPrivate::ArrayInfoType::Reset()
{
  if ( ! this->Name.empty() )
    {
    this->Name.erase( this->Name.begin(), this->Name.end() );
    }
  this->Components = 0;
  this->GlomType = -1;
  this->Status = 0;
  this->Source = -1;
  this->OriginalNames.clear();
  this->OriginalIndices.clear();
  this->ObjectTruth.clear();
}

// ------------------------------------------------------- PRIVATE CLASS MEMBERS
vtkCxxRevisionMacro(vtkExodusIIReaderPrivate,"$Revision$");
vtkStandardNewMacro(vtkExodusIIReaderPrivate);
vtkCxxSetObjectMacro(vtkExodusIIReaderPrivate,CachedConnectivity,vtkUnstructuredGrid);

vtkExodusIIReaderPrivate::vtkExodusIIReaderPrivate()
{
  this->Exoid = -1;
  this->ExodusVersion = -1.;

  this->AppWordSize = 8;
  this->DiskWordSize = 8;

  this->Cache = vtkExodusIICache::New();

  this->TimeStep = 0;

  this->GenerateObjectIdArray = 1;
  this->GenerateGlobalElementIdArray = 0;
  this->GenerateGlobalNodeIdArray = 0;
  this->ApplyDisplacements = 1;
  this->DisplacementMagnitude = 1.;

  this->NumberOfCells = 0;
  this->SqueezePoints = 1;
  this->NextSqueezePoint = 0;

  this->CachedConnectivity = 0;

  memset( (void*)&this->ModelParameters, 0, sizeof(this->ModelParameters) );
}

vtkExodusIIReaderPrivate::~vtkExodusIIReaderPrivate()
{
  this->CloseFile();
  this->Cache->Delete();
  this->SetCachedConnectivity( 0 );
}

void vtkExodusIIReaderPrivate::ComputeGridOffsets()
{
  vtkIdType startCell = 0;

  // Order cells in the grid in a way the user expects:
  // - blocks first, then sets.
  // - elements first, then faces, then edges.
  int conntypidx;
  for ( conntypidx = 0; conntypidx < num_conn_types; ++conntypidx )
    {
    int otyp = obj_types[conn_obj_idx_cvt[conntypidx]];
    int obj;
    int objNum;

    if ( CONNTYPE_IS_BLOCK( conntypidx ) )
      {
      objNum = (int) this->BlockInfo[otyp].size();
      for ( obj = 0; obj < objNum; ++obj )
        {
        BlockInfoType* binfop = &this->BlockInfo[otyp][this->SortedObjectIndices[otyp][obj]];
        if ( binfop->Status )
          {
          binfop->GridOffset = startCell;
          startCell += binfop->Size;
          }
        }
      }
    else
      { // Must be a set...
      objNum = (int) this->SetInfo[otyp].size();
      for ( obj = 0; obj < objNum; ++obj )
        {
        SetInfoType* sinfop = &this->SetInfo[otyp][this->SortedObjectIndices[otyp][obj]];
        if ( sinfop->Status )
          {
          sinfop->GridOffset = startCell;
          startCell += sinfop->Size;
          }
        }
      }
    }
  this->NumberOfCells = startCell;
}

int vtkExodusIIReaderPrivate::VerifyIntegrationPointGlom(
  int nn, char** np, vtksys::RegularExpression& re,
  vtkStdString& field, vtkStdString& ele )
{ 
  vtkstd::vector<vtkstd::vector<int> > gpId;
  int max[3] = { 0, 0, 0 };
  int dim = glomIntegrationPointElementDimension( ele );
  for ( int i = 0; i < nn; ++i )
    {
    gpId.push_back( vtkstd::vector<int>() );
    re.find( np[i] );
    vtkStdString gpIdStr = re.match(3);
    int d = 0;
    for ( vtkStdString::iterator it = gpIdStr.begin(); it != gpIdStr.end(); ++it, ++d )
      {
      gpId[i].push_back( *it - '0' );
      }
    if ( dim < 0 )
      {
      dim = d; 
      if ( dim > 3 )
        {
        vtkWarningMacro( "Field \"" << np[i] << "\" has integration dimension " << d << " > 3." );
        return false;
        }
      }
    else if ( dim != d )
      {
      vtkWarningMacro( "Field \"" << np[i] << "\" has integration dimension " << d << " != " << dim << "." );
      return false;
      }
    else
      {
      for ( int j = 0; j < dim; ++j )
        if ( gpId[i][j] > max[j] )
          max[j] = gpId[i][j];
      }
    }
#ifdef VTK_DBG_GLOM
  cout << "  Integration points are " << dim << "-dimensional.\n";
  for ( int i = 0; i < dim; ++i )
    {
    cout << "    " << (max[i]+1) << " integration points along " << char('r' + i) << ".\n";
    }
#endif // VTK_DBG_GLOM
  int npt = 1;
  for ( int i = 0; i < dim; ++i )
    {
    npt *= max[i] + 1;
    }
  bool bad = false;
  if ( npt != nn )
    {
    vtkWarningMacro( "Field \"" << field.c_str() << "\" has " << nn <<
      " entries, but I expected " << npt << " given the integration order." );
    bad = true;
    }
  int e;
  int ef = -1;
  int cnt;
  bool found; 
  if ( dim == 2 )
    {
    for ( int r = 0; r <= max[0]; ++r )
      { 
      for ( int s = 0; s <= max[1]; ++s )
        {
        found = false;
        cnt = 0;
        for ( e = 0; e < nn; ++e )
          {
          if ( gpId[e][0] == r && gpId[e][1] == s )
            {
            found = true;
            ef = e;
            ++cnt;
            }
          }
        if ( !found )
          {
          vtkWarningMacro( "Field \"" << field.c_str() <<
            "\" is missing Gauss point (" << r << ", " << s << ")." );
          }
        else if ( cnt > 1 )
          {
          vtkWarningMacro( "Field \"" << field.c_str() << "\" has " << (cnt-1) <<
            " duplicate(s) of Gauss point (" << r << ", " << s << ")." );
          }
        else if ( npt == nn && (ef != s + r * (max[1]+1) ) )
          {
          vtkWarningMacro( "Field \"" << field.c_str() <<
            "\" has misplaced Gauss point (" << r << ", " << s << ")." );
          bad = true;
          }
        }
      }
    }
  else if ( dim == 3 )
    {
    for ( int r = 0; r <= max[0]; ++r )
      { 
      for ( int s = 0; s <= max[1]; ++s )
        { 
        for ( int t = 0; t <= max[2]; ++t )
          {
          found = false;
          cnt = 0;
          for ( e = 0; e < nn; ++e )
            {
            if ( gpId[e][0] == r && gpId[e][1] == s && gpId[e][2] == t )
              {
              found = true;
              ef = e;
              ++cnt;
              }
            }
          if ( !found )
            {
            vtkWarningMacro( "Field \"" << field.c_str() <<
              "\" is missing Gauss point (" << r << ", " << s << ", " << t << ")." );
            bad = true;
            }
          else if ( cnt > 1 )
            {
            vtkWarningMacro( "Field \"" << field.c_str() << "\" has " << (cnt-1) <<
              " duplicate(s) of Gauss point (" << r << ", " << s << ", " << t << ")." );
            bad = true;
            }
          else if ( npt == nn && (ef != t + (max[2]+1) * ( s + r * (max[1]+1) )) )
            {
            vtkWarningMacro( "Field \"" << field.c_str() <<
              "\" has misplaced Gauss point (" << r << ", " << s << ", " << t << ")." );
            bad = true;
            }
          }
        }
      }
    }
  return ! bad;
}

void vtkExodusIIReaderPrivate::GlomArrayNames( int objtyp, int num_obj, int num_vars, char** var_names, int* truth_tab )
{
  vtksys::RegularExpression reTensor( "(.*)[XxYyZz][XxYyZz]$" );
  vtksys::RegularExpression reVector( "(.*)[XxYyZz]$" );
  vtksys::RegularExpression reGaussP( "(.*)_([^_]*)_GP([0-9]+)$" );

  ArrayInfoType ainfo;
  for ( int i = 0; i < num_vars; ++i )
    {
    char* srcName = var_names[i];
    bool didGlom = true;
    ainfo.Source = vtkExodusIIReaderPrivate::Result;

    if ( reTensor.find( srcName ) )
      {
      if ( i + 1  < num_vars )
        {
        int ii = i;
        int sl = strlen(var_names[i]) - 2;
        while ( ii < num_vars )
          {
          if ( ! reTensor.find( var_names[ii] ) || strncmp( var_names[ii], var_names[i], sl ) )
            break;
          ainfo.OriginalNames.push_back( var_names[ii] );
          ainfo.OriginalIndices.push_back( ii + 1 );
          ++ii;
          }
        ainfo.Components = ii - i;
        if ( ! ainfo.Components || ! glomTruthTabMatch( num_obj, num_vars, truth_tab, ainfo ) )
          {
          didGlom = false;
          }
        else
          {
          reTensor.find( srcName );
          //cout << "Tensor \"" << reTensor.match(1) << "\" has " << (ii-i) << " components\n";
          ainfo.Name = reTensor.match(1);
          ainfo.GlomType = vtkExodusIIReaderPrivate::SymmetricTensor;
          ainfo.Status = 0;
          ainfo.StorageType = VTK_DOUBLE;
          this->ArrayInfo[ objtyp ].push_back( ainfo );
          i = ii - 1; // advance to end of glom
          }
        ainfo.Reset();
        }
      else
        {
        didGlom = false;
        }
      }
    else if ( reVector.find( srcName ) )
      {
      if ( i+1 < num_vars )
        {
        int ii = i;
        while ( ii < num_vars )
          {
          int sl = strlen(var_names[ii]) - 1;
          // Require the strings to be identical except for the final XYZ at the end.
          if ( ! toupper(var_names[ii][sl]) == ('X' + (ii-i)) || strncmp( var_names[ii], var_names[i], sl ) )
            break;
          ainfo.OriginalNames.push_back( var_names[ii] );
          ainfo.OriginalIndices.push_back( ii + 1 );
          ++ii;
          }
        ainfo.Components = ii - i;
        if ( ainfo.Components < 2 || ! glomTruthTabMatch( num_obj, num_vars, truth_tab, ainfo ) )
          {
          didGlom = false;
          }
        else
          {
          //cout << "Vector \"" << reVector.match(1) << "\" has " << (ii - i) << " components\n";
          ainfo.Name = reVector.match(1);
          ainfo.GlomType = ainfo.Components == 2 ? vtkExodusIIReaderPrivate::Vector2 : vtkExodusIIReaderPrivate::Vector3;
          ainfo.Status = 0;
          ainfo.StorageType = VTK_DOUBLE;
          this->ArrayInfo[ objtyp ].push_back( ainfo );
          i = ii - 1; // advance to end of glom
          }
        ainfo.Reset();
        }
      else
        {
        didGlom = false;
        }
      }
    else if ( reGaussP.find( srcName ) )
      {
      if ( i + 1  < num_vars )
        {
        int ii = i;
        vtkStdString field = reGaussP.match( 1 );
        vtkStdString ele = reGaussP.match( 2 );

        while ( ii < num_vars && reGaussP.find( var_names[ii] ) && (reGaussP.match( 1 ) == field) && (reGaussP.match( 2 ) == ele) )
          {
          ainfo.OriginalNames.push_back( var_names[ii] );
          ainfo.OriginalIndices.push_back( ii + 1 );
          ++ii;
          }
        ainfo.Components = ii - i;
        // Check that the names are consistent (i.e., there aren't missing Gauss points, they all have the same dim, etc.)
        if ( this->VerifyIntegrationPointGlom( ii - i, var_names + i, reGaussP, field, ele ) &&
             glomTruthTabMatch( num_obj, num_vars, truth_tab, ainfo ) )
          {
          //cout << "Gauss Points for \"" << field << "\" on " << ele << "-shaped elements has " << (ii-i) << " components\n";
          ainfo.Name = field;
          ainfo.GlomType = vtkExodusIIReaderPrivate::IntegrationPoint;
          ainfo.Status = 0;
          ainfo.StorageType = VTK_DOUBLE;
          this->ArrayInfo[ objtyp ].push_back( ainfo );
          i = ii - 1; // advance to end of glom
          }
        else
          {
          ainfo.Reset();
          for ( ; i < ii; ++i )
            {
            //cout << "Scalar \"" << var_names[i] << "\"\n";
            ainfo.Name = var_names[i];
            ainfo.Source = Result;
            ainfo.Components = 1;
            ainfo.OriginalIndices.push_back( i + 1 );
            ainfo.OriginalNames.push_back( var_names[i] );
            ainfo.GlomType = vtkExodusIIReaderPrivate::Scalar;
            ainfo.StorageType = VTK_DOUBLE;
            ainfo.Status = 0;
            glomTruthTabMatch( num_obj, num_vars, truth_tab, ainfo ); // fill in ainfo.ObjectTruth
            this->ArrayInfo[ objtyp ].push_back( ainfo );
            }
          }
        ainfo.Reset();
        }
      else
        {
        didGlom = false;
        }
      }
    else
      {
      didGlom = false;
      }

    if ( ! didGlom )
      {
      //cout << "Scalar \"" << srcName << "\"\n";
      ainfo.Name = srcName;
      ainfo.Source = Result;
      ainfo.Components = 1;
      ainfo.OriginalIndices.push_back( i + 1 );
      ainfo.OriginalNames.push_back( var_names[i] );
      ainfo.GlomType = vtkExodusIIReaderPrivate::Scalar;
      ainfo.StorageType = VTK_DOUBLE;
      ainfo.Status = 0;
      glomTruthTabMatch( num_obj, num_vars, truth_tab, ainfo ); // fill in ainfo.ObjectTruth
      this->ArrayInfo[ objtyp ].push_back( ainfo );
      ainfo.Reset();
      }
    }
}

int vtkExodusIIReaderPrivate::AssembleOutputConnectivity( vtkIdType timeStep, vtkUnstructuredGrid* output )
{
  output->Reset(); // FIXME: Don't think I need this, since we ShallowCopy over it... right?
  if ( this->CachedConnectivity )
    {
    output->ShallowCopy( this->CachedConnectivity );
    return 1;
    }

  // OK, we needed to remake the cache...
  this->CachedConnectivity = vtkUnstructuredGrid::New();
  this->CachedConnectivity->Allocate( this->NumberOfCells );
  if ( this->SqueezePoints )
    {
    this->NextSqueezePoint = 0;
    this->PointMap.clear();
    this->PointMap.reserve( this->ModelParameters.num_nodes );
    for ( int i = 0; i < this->ModelParameters.num_nodes; ++i )
      {
      this->PointMap.push_back( -1 );
      }
    }

  // Need to assemble connectivity array from smaller ones.
  // Call GetCacheOrRead() for each smaller array

  // Might want to experiment with the effectiveness of caching connectivity... set up the
  //   ExodusIICache class with the ability to never cache some key types.
  // Might also want to experiment with policies other than LRU, especially applied to
  //   arrays that are not time-varying. During animations, they will most likely get
  //   dropped even though that might not be wise.

  // Loop over all the block and set types which could generate connectivity information
  // in an order that the user expects (element blocks first, blocks ordered by block ID,
  // not file order).
  int conntypidx;
  int nbl = 0;
  for ( conntypidx = 0; conntypidx < num_conn_types; ++conntypidx )
    {
    int otyp = obj_types[conn_obj_idx_cvt[conntypidx]];
    // Loop over all blocks/sets of this type
    int numObj = this->GetNumberOfObjectsOfType( otyp );
    int obj;
    int sortIdx;
    for ( sortIdx = 0; sortIdx < numObj; ++sortIdx )
      {
      if ( ! this->GetObjectStatus( otyp, sortIdx ) )
        continue;

      obj = this->SortedObjectIndices[otyp][sortIdx]; // Preserve the "sorted" order when concatenating
      if ( CONNTYPE_IS_BLOCK(conntypidx) )
        {
        this->InsertBlockCells( otyp, obj, conn_types[conntypidx], timeStep, this->CachedConnectivity );
        }
      else if ( CONNTYPE_IS_SET(conntypidx) )
        {
        this->InsertSetCells( otyp, obj, conn_types[conntypidx], timeStep, this->CachedConnectivity );
        }
      else
        {
        vtkErrorMacro( "Bad connectivity object type. Harass the responsible programmer." );
        }

      ++nbl;
      }
    }

  // OK, now copy our cache to the output...
  output->ShallowCopy( this->CachedConnectivity );
  //this->CachedConnectivity->ShallowCopy( output );
  if ( this->SqueezePoints )
    {
    vtkDebugMacro( << "Squeezed down to " << this->NextSqueezePoint << " points\n" );
    }
  return 0;
}

int vtkExodusIIReaderPrivate::AssembleOutputPoints( vtkIdType timeStep, vtkUnstructuredGrid* output )
{
  (void)timeStep;
  vtkPoints* pts = output->GetPoints();
  if ( ! pts )
    {
    pts = vtkPoints::New();
    output->SetPoints( pts );
    pts->FastDelete();
    }
  else
    {
    pts->Reset();
    }

  int ts = -1; // If we don't have displacements, only cache the array under one key.
  if ( this->ApplyDisplacements && this->FindDisplacementVectors( timeStep ) )
    { // Otherwise, each time step's array will be different.
    ts = timeStep;
    }

  vtkDataArray* arr = this->GetCacheOrRead( vtkExodusIICacheKey( ts, vtkExodusIIReader::NODAL_COORDS, 0, 0 ) );
  if ( ! arr )
    {
    vtkErrorMacro( "Unable to read points from file." );
    return 0;
    }

  if ( this->SqueezePoints )
    {
    vtkIdType exoPtId;
    pts->SetNumberOfPoints( this->NextSqueezePoint );
    for ( exoPtId = 0; exoPtId < this->ModelParameters.num_nodes; ++exoPtId )
      {
      vtkIdType outPtId = this->PointMap[exoPtId];
      if ( outPtId >= 0 )
        {
        pts->SetPoint( outPtId, arr->GetTuple( exoPtId ) );
        }
      }
    }
  else
    {
    pts->SetData( arr );
    }
  return 1;
}

int vtkExodusIIReaderPrivate::AssembleOutputPointArrays( vtkIdType timeStep, vtkUnstructuredGrid* output )
{
  int status = 1;
  vtkstd::vector<ArrayInfoType>::iterator ai;
  int aidx = 0;

  for (
    ai = this->ArrayInfo[ vtkExodusIIReader::NODAL ].begin();
    ai != this->ArrayInfo[ vtkExodusIIReader::NODAL ].end();
    ++ai, ++aidx )
    {
    if ( ! ai->Status )
      continue; // Skip arrays we don't want.

    vtkExodusIICacheKey key( timeStep, vtkExodusIIReader::NODAL, 0, aidx );
    vtkDataArray* src = this->GetCacheOrRead( key );
    if ( !src )
      {
      vtkWarningMacro( "Unable to read point array " << ai->Name.c_str() << " at time step " << timeStep );
      status = 0;
      continue;
      }

    this->AddPointArray( src, output );
    }
  return status;
}

int vtkExodusIIReaderPrivate::AssembleOutputCellArrays( vtkIdType timeStep, vtkUnstructuredGrid* output )
{
  // Need to assemble arrays from smaller per-block/set arrays.
  // Call GetCacheOrRead() for each smaller array

  // Step 1. Create the large arrays and fill them (but don't pad them).
  vtkCellData* cd = output->GetCellData();
  vtkstd::map<int,vtkstd::vector<ArrayInfoType> >::iterator ami;
  for ( ami = this->ArrayInfo.begin(); ami != this->ArrayInfo.end(); ++ami )
    {
    if ( ami->first == vtkExodusIIReader::NODAL || ami->first == vtkExodusIIReader::NODE_MAP )
      continue; // we handle nodal arrays in AssembleOutputPointArrays

    // See if any objects of this type are turned on (Status != 0)
    int obj;
    int numObjOn = 0;
    int numObj = this->GetNumberOfObjectsOfType( ami->first );
    for ( obj = 0; obj < numObj; ++obj )
      {
      if ( this->GetObjectStatus( ami->first, obj ) )
        {
        ++numObjOn;
        }
      }
    if ( numObjOn == 0 )
      continue; // this array may be on, but no objects of this type are active... skip it.

    vtkstd::vector<ArrayInfoType>::iterator ai;
    int aidx = 0;
    for ( ai = ami->second.begin(); ai != ami->second.end(); ++ai, ++aidx )
      {
      if ( ! ai->Status )
        continue;

      vtkDataArray* arr = cd->GetArray( ai->Name.c_str() );
      if ( arr )
        {
        // OK, we've already created this array for some other type of object, 
        // so now we have to make sure the arrays are consistent. If not, we
        // turn off the second one we encounter. The user can disable the first
        // and re-enable the second if required.
        if ( arr->GetDataType() != ai->StorageType )
          {
          vtkErrorMacro( "Cell array \"" << ai->Name.c_str() << "\" has conflicting types across blocks/sets." );
          ai->Status = 0; // Don't load this block's/set's version. User must disable other block/set before loading this one.
          arr = 0;
          }
        if ( arr && (arr->GetNumberOfComponents() != ai->Components) )
          {
          vtkErrorMacro( "Cell array \"" << ai->Name.c_str() << "\" has different number of components across blocks/sets." );
          ai->Status = 0; // Don't load this block's/set's version. User must disable other block/set before loading this one.
          arr = 0;
          }
        }
      else
        {
        // Re-use an existing or create a new array
        vtkExodusIICacheKey key( ai->Source == Result ? timeStep : -1, vtkExodusIIReader::GLOBAL, ami->first, aidx );
        arr = this->Cache->Find( key );
        if ( arr )
          { // Existing array was in cache
          cd->AddArray( arr );
          continue;
          }
        arr = vtkDataArray::CreateDataArray( ai->StorageType );
        arr->SetName( ai->Name.c_str() );
        arr->SetNumberOfComponents( ai->Components );
        arr->SetNumberOfTuples( this->NumberOfCells );
        cd->AddArray( arr );
        this->Cache->Insert( key, arr );
        arr->FastDelete();
        }

      if ( ! arr )
        {
        continue;
        }

      // OK, the array exists and has the correct number of tuples. Loop over all objects of
      // this type and insert their values into the global cell array according to their GridOffset.
      int otypidx = this->GetObjectTypeIndexFromObjectType( ami->first );
      BlockSetInfoType* bsinfop;
      vtkDataArray* src;
      for ( obj = 0; obj < numObj; ++obj )
        {

        if ( ! ai->ObjectTruth[obj] )
          { // skip blocks for which this array doesn't exist.
          continue;
          }

        src = 0;
        if ( OBJTYPE_IS_BLOCK( otypidx ) )
          {

          BlockInfoType* binfop = &this->BlockInfo[ami->first][obj];
          bsinfop = (BlockSetInfoType*) binfop;
          if ( binfop->Status )
            {
            src = this->GetCacheOrRead( vtkExodusIICacheKey( timeStep, ami->first, obj, aidx ) );
            if ( src )
              {
              vtkIdType c;
              for ( c = 0; c < binfop->Size; ++c )
                {
                cd->CopyTuple( src, arr, c, c + binfop->GridOffset );
                }
              }
            }

          }
        else if ( OBJTYPE_IS_SET( otypidx ) )
          {

          SetInfoType* sinfop = &this->SetInfo[ami->first][obj];
          bsinfop = (BlockSetInfoType*) sinfop;
          if ( sinfop->Status )
            {
            src = this->GetCacheOrRead( vtkExodusIICacheKey( timeStep, ami->first, obj, aidx ) );
            if ( src )
              {
              vtkIdType c;
              for ( c = 0; c < sinfop->Size; ++c )
                {
                cd->CopyTuple( src, arr, c, c + sinfop->GridOffset );
                }
              }
            }

          }
        else
          {
          vtkErrorMacro( "Array defined for an unknown type of object: " << ami->first <<
            " with index: " << otypidx << ". Skipping." );
          continue;
          }

        if ( ! src && bsinfop && bsinfop->Status )
          {
          vtkErrorMacro( "Cell array \"" << ai->Name.c_str() << "\" not defined on " << objtype_names[otypidx] <<
            " " << bsinfop->Id << " but truth table claimed it was. Fixing truth table in memory (not in file).");
          ai->ObjectTruth[obj] = 0;
          }
        }

      }
    }

  // Step 2. Now that we have very carefully created an array with a storage
  // type and number of components that match the arrays whose status is 1,
  // loop over the objects whose status is 1 but that do not have an
  // an array status of 1 or who have truth table set to 0. These objects
  // need to pad the arrays with zeros.
  int otypidx;
  for ( otypidx = 0; obj_types[otypidx] != vtkExodusIIReader::NODE_MAP; ++otypidx )
    {
    int otyp = obj_types[otypidx];
    int obj;
    int numObj = this->GetNumberOfObjectsOfType( otyp );

    int ai;
    for ( ai = 0; ai < cd->GetNumberOfArrays(); ++ai )
      {
      vtkDataArray* arr = cd->GetArray( ai );
      ArrayInfoType* ainfop = this->FindArrayInfoByName( otyp, arr->GetName() );

      for ( obj = 0; obj < numObj; ++obj )
        {
        BlockSetInfoType* bsinfop = (BlockSetInfoType*) this->GetObjectInfo( otypidx, obj );

        if (
          bsinfop && bsinfop->Status &&
          ( !ainfop || ! ainfop->Status || ( ainfop->Status && ! ainfop->ObjectTruth[obj] ) )
        )
          {
          vtkstd::vector<double> zedTuple( arr->GetNumberOfComponents(), 0. ); // an empty tuple used to pad arrays
          vtkIdType i;
          vtkIdType c = bsinfop->GridOffset;
          vtkDebugMacro( << arr->GetName() << ": Padding " << bsinfop->Size << " cells at " << c << "\n" );
          for ( i = 0; i < bsinfop->Size; ++i, ++c )
            {
            arr->SetTuple( c, &zedTuple[0] );
            }
          }
        }
      }
    }

  return 1;
}

int vtkExodusIIReaderPrivate::AssembleOutputPointMaps( vtkIdType timeStep, vtkUnstructuredGrid* output )
{
  (void)timeStep;
  int status = 1;
  vtkstd::vector<MapInfoType>::iterator mi;
  int midx = 0;

  for (
    mi = this->MapInfo[ vtkExodusIIReader::NODE_MAP ].begin();
    mi != this->MapInfo[ vtkExodusIIReader::NODE_MAP ].end();
    ++mi, ++midx )
    {
    if ( ! mi->Status )
      continue; // Skip arrays we don't want.

    vtkExodusIICacheKey key( -1, vtkExodusIIReader::NODE_MAP, 0, midx );
    vtkDataArray* src = this->GetCacheOrRead( key );
    if ( !src )
      {
      vtkWarningMacro( "Unable to read point map array \"" << mi->Name.c_str() << "\" (" << midx << ")" );
      status = 0;
      continue;
      }

    this->AddPointArray( src, output );
    }
  return status;
}

int vtkExodusIIReaderPrivate::AssembleOutputCellMaps( vtkIdType timeStep, vtkUnstructuredGrid* output )
{
  (void)timeStep;
  // Step 1. Create the large arrays and fill them (but don't pad them).
  vtkCellData* cd = output->GetCellData();
  vtkstd::map<int,vtkstd::vector<MapInfoType> >::iterator mmi;
  for ( mmi = this->MapInfo.begin(); mmi != this->MapInfo.end(); ++mmi )
    {
    if ( mmi->first == vtkExodusIIReader::NODAL || mmi->first == vtkExodusIIReader::NODE_MAP )
      continue; // we handle nodal arrays in AssembleOutputPointMaps

    // See if any maps of this type are turned on (Status != 0)
    int obj;
    int numObjOn = 0;
    int numObj = this->GetNumberOfObjectsOfType( mmi->first );
    for ( obj = 0; obj < numObj; ++obj )
      {
      if ( this->GetObjectStatus( mmi->first, obj ) )
        {
        ++numObjOn;
        break; // know we know we need the array
        }
      }
    if ( numObjOn == 0 )
      continue; // this array may be on, but no objects of this type are active... skip it.

    vtkstd::vector<MapInfoType>::iterator mi;
    int midx = 0;
    for ( mi = mmi->second.begin(); mi != mmi->second.end(); ++mi, ++midx )
      {
      if ( ! mi->Status )
        continue;

      vtkDataArray* arr = cd->GetArray( mi->Name.c_str() );
      if ( arr )
        {
        // OK, we've already created this array for some other type of object, 
        // so now we have to make sure the arrays are consistent. If not, we
        // turn off the second one we encounter. The user can disable the first
        // and re-enable the second if required.
        if ( arr->GetDataType() != VTK_ID_TYPE )
          {
          vtkErrorMacro( "Cell array \"" << mi->Name.c_str() << "\" has conflicting types." );
          mi->Status = 0; // Don't load this map. User must disable other array before loading this one.
          arr = 0;
          }
        if ( arr && (arr->GetNumberOfComponents() != 1) )
          {
          vtkErrorMacro( "Cell array \"" << mi->Name.c_str() << "\" has different number of components than map requires." );
          mi->Status = 0; // Don't load this block's/set's version. User must disable other block/set before loading this one.
          arr = 0;
          }
        }
      else
        {
        // Create the array
        arr = vtkIdTypeArray::New();
        arr->SetName( mi->Name.c_str() );
        arr->SetNumberOfComponents( 1 );
        arr->SetNumberOfTuples( this->NumberOfCells );
        // Eliminate the second pass that pads cells by initializing the entire array here.
        memset( arr->GetVoidPointer(0), 0, this->NumberOfCells * sizeof(vtkIdType) );
        cd->AddArray( arr );
        arr->FastDelete();
        }

      if ( ! arr )
        {
        continue;
        }

      // OK, the array exists and has the correct number of tuples. Loop over all objects of
      // this type and insert their values into the global cell array according to their GridOffset.
      int otyp = this->GetObjectTypeFromMapType( mmi->first );
      BlockInfoType* binfop;
      vtkDataArray* src;
      int numBlk = (int) this->BlockInfo[otyp].size();
      int blk;

      src = this->GetCacheOrRead( vtkExodusIICacheKey( -1, mmi->first, 0, midx ) );
      if ( src )
        {
        for ( blk = 0; blk < numBlk; ++blk )
          {
          binfop = &this->BlockInfo[otyp][blk];
          if ( ! binfop->Status )
            continue;

          vtkIdType c;
          for ( c = 0; c < binfop->Size; ++c )
            {
            cd->CopyTuple( src, arr, c + binfop->FileOffset - 1, c + binfop->GridOffset );
            }
          }
        }

      // ===
      }
    }
  return 1;
}

int vtkExodusIIReaderPrivate::AssembleOutputProceduralArrays( vtkIdType timeStep, vtkUnstructuredGrid* output )
{
  (void)timeStep;
  int status = 7;
  if ( this->GenerateObjectIdArray )
    {
    vtkExodusIICacheKey key( -1, vtkExodusIIReader::GLOBAL_OBJECT_ID, 0, 0 );
    vtkDataArray* arr = this->GetCacheOrRead( key );
    vtkCellData* cd = output->GetCellData();
    if ( arr )
      {
      cd->AddArray( arr );
      status -= 1;
      }
    }

  if ( this->GenerateGlobalElementIdArray )
    {
    // This retrieves the first new-style map, or if that is not present,
    // the solitary old-style map (which always exists but may be
    // procedurally generated if it is not stored with the file).
    vtkExodusIICacheKey key( -1, vtkExodusIIReader::GLOBAL_ELEMENT_ID, 0, 0 );
    vtkDataArray* arr = this->GetCacheOrRead( key );
    vtkCellData* cd = output->GetCellData();
    if ( arr )
      {
      vtkDataArray* ped = vtkIdTypeArray::New();
      ped->DeepCopy( arr );
      ped->SetName( vtkExodusIIReader::GetPedigreeElementIdArrayName() );

      cd->AddArray( ped );
      cd->SetGlobalIds( arr );
      ped->FastDelete();

      status -= 2;
      }
    }

  if ( this->GenerateGlobalNodeIdArray )
    {
    // This retrieves the first new-style map, or if that is not present,
    // the solitary old-style map (which always exists but may be
    // procedurally generated if it is not stored with the file).
    vtkExodusIICacheKey key( -1, vtkExodusIIReader::GLOBAL_NODE_ID, 0, 0 );
    vtkDataArray* arr = this->GetCacheOrRead( key );
    vtkPointData* pd = output->GetPointData();
    if ( arr )
      {
      vtkDataArray* ped = vtkIdTypeArray::New();
      ped->DeepCopy( arr );
      ped->SetName( vtkExodusIIReader::GetPedigreeNodeIdArrayName() );

      pd->AddArray( ped );
      pd->SetGlobalIds( arr );
      ped->FastDelete();

      status -= 4;
      }
    }

  return status;
}


void vtkExodusIIReaderPrivate::InsertBlockCells( int otyp, int obj, int conn_type, int timeStep, vtkUnstructuredGrid* output )
{
  (void)timeStep;
  BlockInfoType* binfo = &this->BlockInfo[otyp][obj];
  if ( binfo->Size == 0 )
    {
    // No entries in this block. 
    // This happens in parallel filesets when all elements are distributed to other files.
    // Silently ignore.
    return;
    }

  vtkIntArray* arr;
  arr = vtkIntArray::SafeDownCast( this->GetCacheOrRead( vtkExodusIICacheKey( -1, conn_type, obj, 0 ) ) );
  if ( ! arr )
    {
    vtkWarningMacro( "Block wasn't present in file? Working around it. Expect trouble." );
    binfo->Status = 0;
    this->ComputeGridOffsets();
    return;
    }

  if ( this->SqueezePoints )
    {
    vtkstd::vector<vtkIdType> cellIds;
    cellIds.resize( binfo->PointsPerCell );
    int* srcIds = arr->GetPointer( 0 );

    for ( int i = 0; i < binfo->Size; ++i )
      {
      for ( int p = 0; p < binfo->PointsPerCell; ++p )
        {
        cellIds[p] = this->GetSqueezePointId( srcIds[p] );
        //cout << " " << srcIds[p] << "(" << cellIds[p] << ")";
        }
      //cout << "\n";
      //cout << " " <<
      output->InsertNextCell( binfo->CellType, binfo->PointsPerCell, &cellIds[0] );
      srcIds += binfo->PointsPerCell;
      }
      //cout << "\n";
    }
  else
    {
#ifdef VTK_USE_64BIT_IDS
    vtkstd::vector<vtkIdType> cellIds;
    cellIds.resize( binfo->PointsPerCell );
    int* srcIds = arr->GetPointer( 0 );

    for ( int i = 0; i < binfo->Size; ++i )
      {
      for ( int p = 0; p < binfo->PointsPerCell; ++p )
        {
        cellIds[p] = srcIds[p];
        //cout << " " << srcIds[p];
        }
      //cout << "\n";
      output->InsertNextCell( binfo->CellType, binfo->PointsPerCell, &cellIds[0] );
      srcIds += binfo->PointsPerCell;
      }
#else // VTK_USE_64BIT_IDS
    vtkIdType* srcIds = (vtkIdType*) arr->GetPointer( 0 );
    
    for ( int i = 0; i < binfo->Size; ++i )
      {
      output->InsertNextCell( binfo->CellType, binfo->PointsPerCell, srcIds );
      srcIds += binfo->PointsPerCell;
      //for ( int k = 0; k < binfo->PointsPerCell; ++k )
        //cout << " " << srcIds[k];
      //cout << "\n";
      }
#endif // VTK_USE_64BIT_IDS
    }
}

void vtkExodusIIReaderPrivate::InsertSetCells( int otyp, int obj, int conn_type, int timeStep, vtkUnstructuredGrid* output )
{
  (void)timeStep;
  SetInfoType* sinfo = &this->SetInfo[otyp][obj];
  if ( sinfo->Size == 0 )
    {
    // No entries in this set. 
    // This happens in parallel filesets when all elements are distributed to other files.
    // Silently ignore.
    return;
    }

  vtkIntArray* arr = vtkIntArray::SafeDownCast( this->GetCacheOrRead( vtkExodusIICacheKey( -1, conn_type, obj, 0 ) ) );
  if ( ! arr )
    {
    vtkWarningMacro( "Set wasn't present in file? Working around it. Expect trouble." );
    sinfo->Status = 0;
    this->ComputeGridOffsets();
    return;
    }

  switch ( otyp )
    {
  case vtkExodusIIReader::NODE_SET:
    // Easy
    this->InsertSetNodeCopies( arr, otyp, obj, output );
    break;
  case vtkExodusIIReader::EDGE_SET:
    // Not so fun. We must copy cells from possibly many edge blocks.
    this->InsertSetCellCopies( arr, vtkExodusIIReader::EDGE_BLOCK, obj, output );
    break;
  case vtkExodusIIReader::FACE_SET:
    // Not so fun. We must copy cells from possibly many face blocks.
    this->InsertSetCellCopies( arr, vtkExodusIIReader::FACE_BLOCK, obj, output );
    break;
  case vtkExodusIIReader::SIDE_SET:
    // Way hard even when we let Exodus do a lot for us.
    this->InsertSetSides( arr, otyp, obj, output );
    break;
  case vtkExodusIIReader::ELEM_SET:
    // Not so fun. We must copy cells from possibly many element blocks.
    this->InsertSetCellCopies( arr, vtkExodusIIReader::ELEM_BLOCK, obj, output );
    break;
    }
}

void vtkExodusIIReaderPrivate::AddPointArray( vtkDataArray* src, vtkUnstructuredGrid* output )
{
  vtkPointData* pd = output->GetPointData();
  if ( this->SqueezePoints )
    {
    // subset the array using PointMap
    vtkDataArray* dest = vtkDataArray::CreateDataArray( src->GetDataType() );
    dest->SetName( src->GetName() );
    dest->SetNumberOfComponents( src->GetNumberOfComponents() );
    dest->SetNumberOfTuples( this->NextSqueezePoint );
    vtkIdType exoPtId;
    for ( exoPtId = 0; exoPtId < this->ModelParameters.num_nodes; ++exoPtId )
      {
      vtkIdType outPtId = this->PointMap[exoPtId];
      if ( outPtId >= 0 )
        {
        pd->CopyTuple( src, dest, exoPtId, outPtId );
        }
      }
    pd->AddArray( dest );
    dest->FastDelete();
    }
  else
    {
    pd->AddArray( src );
    }
}

void vtkExodusIIReaderPrivate::InsertSetNodeCopies( vtkIntArray* refs, int otyp, int obj, vtkUnstructuredGrid* output )
{
  (void)otyp;
  (void)obj;
  // Insert a "VERTEX" cell for each node in the set.
  vtkIdType ref;
  vtkIdType tmp;
  int* iptr = refs->GetPointer( 0 );
  for ( ref = 0; ref < refs->GetNumberOfTuples(); ++ref, ++iptr )
    {
    tmp = *iptr;
    output->InsertNextCell( VTK_VERTEX, 1, &tmp );
    }
}

void vtkExodusIIReaderPrivate::InsertSetCellCopies( vtkIntArray* refs, int otyp, int obj, vtkUnstructuredGrid* output )
{
  (void)obj;
  // First, sort the set by entry number (element, face, or edge ID)
  // so that we can refer to each block just once as we process cells.
  vtkSortDataArray::SortArrayByComponent( refs, 0 );
  refs->Register( this ); // Don't let the cache delete this array when we fetch others...

  vtkIdType nrefs = refs->GetNumberOfTuples();
  vtkIdType ref = 0;
  vtkIdType bnum = -1;
  vtkIdType lastBlockEntry = -1;
  int* pref = refs->GetPointer( 0 );
  int stride = refs->GetNumberOfComponents();
  BlockInfoType* binfop = 0; //&this->BlockInfo[otyp][bnum];
  int* nodeconn = 0;
  vtkIdType* cellConn;
  int nnpe = 0;
  vtkIntArray* nconn;
  vtkstd::vector<vtkIdType> tmpTuple;
  while ( ref < nrefs )
    {
    int loadNewBlk = 0;
    while ( pref[0] >= lastBlockEntry )
      { // advance to the next block (always true first time through parent loop)
      ++bnum;
      if ( bnum >= (int) this->BlockInfo[otyp].size() )
        return;
      binfop = &this->BlockInfo[otyp][bnum];
      lastBlockEntry = binfop->FileOffset + binfop->Size - 1;
      loadNewBlk = 1;
      }
    if ( loadNewBlk )
      {
      nconn = vtkIntArray::SafeDownCast(
        this->GetCacheOrRead( vtkExodusIICacheKey( -1, this->GetBlockConnTypeFromBlockType( otyp ), bnum, 0 ) )
        );
      if ( ! nconn )
        {
        vtkErrorMacro( "Unable to read block \"" << binfop->Name.c_str() << "\" (" << binfop->Id << ")" );
        break;
        }
      nodeconn = nconn->GetPointer( 0 );
      nnpe = nconn->GetNumberOfComponents();
      if ( stride > 1 || this->SqueezePoints )
        {
        tmpTuple.resize( nnpe );
        }
      }

    if ( stride > 1 && pref[1] < 0 )
      { // negative orientation => reverse cell connectivity
      vtkIdType off = (pref[0] + 2 - binfop->FileOffset) * nnpe - 1;
      for ( int k = 0; k < nnpe; ++k )
        tmpTuple[k] = nodeconn[off-k];
      cellConn = &tmpTuple[0];
      }
    else
#ifndef VTK_USE_64BIT_IDS
      if ( this->SqueezePoints )
#endif // VTK_USE_64BIT_IDS
      {
      vtkIdType off = (pref[0] + 1 - binfop->FileOffset) * nnpe;
      for ( int k = 0; k < nnpe; ++k )
        tmpTuple[k] = nodeconn[off+k];
      cellConn = &tmpTuple[0];
      }
#ifndef VTK_USE_64BIT_IDS
    else
      {
      cellConn = (int*)nodeconn + (pref[0] + 1 - binfop->FileOffset) * nnpe;
      }
#endif // VTK_USE_64BIT_IDS

    if ( this->SqueezePoints )
      { // this loop is separated out to handle case (stride > 1 && pref[1] < 0 && this->SqueezePoints)
      for ( int k = 0; k < nnpe; ++k )
        {
        vtkIdType* x = &this->PointMap[cellConn[k]];
        if ( *x < 0 )
          {
          *x = this->NextSqueezePoint++;
          }
        cellConn[k] = *x;
        }
      }

    output->InsertNextCell( binfop->CellType, nnpe, cellConn );

    pref += stride;
    ++ref;
    }

  refs->UnRegister( this );
}

void vtkExodusIIReaderPrivate::InsertSetSides( vtkIntArray* refs, int otyp, int obj, vtkUnstructuredGrid* output )
{
  static const int sideCellTypes[] =
    {
    VTK_EMPTY_CELL, // don't support any cells with 0 nodes per side
    VTK_VERTEX,
    VTK_LINE,
    VTK_TRIANGLE,
    VTK_QUAD,
    VTK_EMPTY_CELL, // don't support any cells with 5 nodes per side
    VTK_QUADRATIC_TRIANGLE,
    VTK_EMPTY_CELL, // don't support any cells with 7 nodes per side
    VTK_QUADRATIC_QUAD,
    VTK_BIQUADRATIC_QUAD
    };

  int numSides = this->SetInfo[otyp][obj].Size;
  int* nodesPerSide = refs->GetPointer( 0 );
  int* sideNodes = nodesPerSide + numSides;
  vtkstd::vector<vtkIdType> cellConn;
  cellConn.resize( 9 );

  if ( this->SqueezePoints )
    {
    int nnpe;
    for ( int side = 0; side < numSides; ++side )
      {
      nnpe = nodesPerSide[side];
      for ( int k = 0; k < nnpe; ++k )
        {
        vtkIdType* x = &this->PointMap[sideNodes[k]];
        if ( *x < 0 )
          {
          *x = this->NextSqueezePoint++;
          }
        cellConn[k] = *x;
        }
      output->InsertNextCell( sideCellTypes[nnpe], nnpe, &cellConn[0] );
      sideNodes += nnpe;
      }
    }
  else
    {
    int nnpe;
    for ( int side = 0; side < numSides; ++side )
      {
      nnpe = nodesPerSide[side];
#ifdef VTK_USE_64BIT_IDS
      for ( int k = 0; k < nnpe; ++k )
        {
          cellConn[k] = sideNodes[k];
        }
      output->InsertNextCell( sideCellTypes[nnpe], nnpe, &cellConn[0] );
#else // VTK_USE_64BIT_IDS
      output->InsertNextCell( sideCellTypes[nnpe], nnpe, sideNodes );
#endif // VTK_USE_64BIT_IDS
      sideNodes += nnpe;
      }
    }
}

vtkDataArray* vtkExodusIIReaderPrivate::GetCacheOrRead( vtkExodusIICacheKey key )
{
  vtkDataArray* arr = this->Cache->Find( key );

  if ( arr )
    {
    //
    return arr;
    }

  int exoid = this->Exoid;

  // If array is NULL, try reading it from file.
  if ( key.ObjectType == vtkExodusIIReader::GLOBAL )
    {
    // need to assemble result array from smaller ones.
    // call GetCacheOrRead() for each smaller array
    // pay attention to SqueezePoints
    }
  else if ( key.ObjectType == vtkExodusIIReader::NODAL )
    {
    // read nodal array
    ArrayInfoType* ainfop = &this->ArrayInfo[vtkExodusIIReader::NODAL][key.ArrayId];
    arr = vtkDataArray::CreateDataArray( ainfop->StorageType );
    arr->SetName( ainfop->Name.c_str() );
    arr->SetNumberOfComponents( ainfop->Components );
    arr->SetNumberOfTuples( this->ModelParameters.num_nodes );
    if ( ainfop->Components == 1 )
      {
      if ( ex_get_var( exoid, key.Time + 1, key.ObjectType,
          ainfop->OriginalIndices[0], 0, arr->GetNumberOfTuples(),
          arr->GetVoidPointer( 0 ) ) < 0 )
        {
        vtkErrorMacro( "Could not read nodal result variable " << ainfop->Name.c_str() << "." );
        arr->Delete();
        arr = 0;
        }
      }
    else
      {
      // Exodus doesn't support reading with a stride, so we have to manually interleave the arrays. Bleh.
      vtkstd::vector<vtkstd::vector<double> > tmpVal;
      tmpVal.resize( ainfop->Components );
      int c;
      for ( c = 0; c < ainfop->Components; ++c )
        {
        vtkIdType N = this->ModelParameters.num_nodes;
        tmpVal[c].resize( N );
        if ( ex_get_var( exoid, key.Time + 1, key.ObjectType,
            ainfop->OriginalIndices[c], 0, arr->GetNumberOfTuples(),
            &tmpVal[c][0] ) < 0)
          {
          vtkErrorMacro( "Could not read nodal result variable " << ainfop->OriginalNames[c].c_str() << "." );
          arr->Delete();
          arr = 0;
          return 0;
          }
        }
      int t;
      vtkstd::vector<double> tmpTuple;
      tmpTuple.resize( ainfop->Components );
      for ( t = 0; t < arr->GetNumberOfTuples(); ++t )
        {
        for ( c = 0; c < ainfop->Components; ++c )
          {
          tmpTuple[c] = tmpVal[c][t];
          }
        arr->SetTuple( t, &tmpTuple[0] );
        }
      }
    }
  else if (
    key.ObjectType == vtkExodusIIReader::EDGE_BLOCK ||
    key.ObjectType == vtkExodusIIReader::FACE_BLOCK ||
    key.ObjectType == vtkExodusIIReader::ELEM_BLOCK ||
    key.ObjectType == vtkExodusIIReader::NODE_SET ||
    key.ObjectType == vtkExodusIIReader::EDGE_SET ||
    key.ObjectType == vtkExodusIIReader::FACE_SET ||
    key.ObjectType == vtkExodusIIReader::SIDE_SET ||
    key.ObjectType == vtkExodusIIReader::ELEM_SET
    )
    {
    int otypidx = this->GetObjectTypeIndexFromObjectType( key.ObjectType );
    ArrayInfoType* ainfop = &this->ArrayInfo[key.ObjectType][key.ArrayId];
    ObjectInfoType* oinfop = this->GetObjectInfo( otypidx, key.ObjectId );

    arr = vtkDataArray::CreateDataArray( ainfop->StorageType );
    arr->SetName( ainfop->Name.c_str() );
    arr->SetNumberOfComponents( ainfop->Components );
    arr->SetNumberOfTuples( oinfop->Size );
    if ( ainfop->Components == 1 )
      {
      if ( ex_get_var( exoid, key.Time + 1, key.ObjectType,
          ainfop->OriginalIndices[0], oinfop->Id, arr->GetNumberOfTuples(),
          arr->GetVoidPointer( 0 ) ) < 0)
        {
        vtkErrorMacro( "Could not read result variable " << ainfop->Name.c_str() <<
          " for " << objtype_names[otypidx] << " " << oinfop->Id << "." );
        arr->Delete();
        arr = 0;
        }
      }
    else
      {
      // Exodus doesn't support reading with a stride, so we have to manually interleave the arrays. Bleh.
      vtkstd::vector<vtkstd::vector<double> > tmpVal;
      tmpVal.resize( ainfop->Components );
      int c;
      for ( c = 0; c < ainfop->Components; ++c )
        {
        vtkIdType N = arr->GetNumberOfTuples();
        tmpVal[c].resize( N );
        if ( ex_get_var( exoid, key.Time + 1, key.ObjectType,
            ainfop->OriginalIndices[c], oinfop->Id, arr->GetNumberOfTuples(),
            &tmpVal[c][0] ) < 0)
          {
          vtkErrorMacro( "Could not read result variable " << ainfop->OriginalNames[c].c_str() <<
            " for " << objtype_names[otypidx] << " " << oinfop->Id << "." );
          arr->Delete();
          arr = 0;
          }
        }
      int t;
      vtkstd::vector<double> tmpTuple;
      tmpTuple.resize( ainfop->Components );
      for ( t = 0; t < arr->GetNumberOfTuples(); ++t )
        {
        for ( c = 0; c < ainfop->Components; ++c )
          {
          tmpTuple[c] = tmpVal[c][t];
          }
        arr->SetTuple( t, &tmpTuple[0] );
        }
      }
    }
  else if (
    key.ObjectType == vtkExodusIIReader::NODE_MAP ||
    key.ObjectType == vtkExodusIIReader::EDGE_MAP ||
    key.ObjectType == vtkExodusIIReader::FACE_MAP ||
    key.ObjectType == vtkExodusIIReader::ELEM_MAP
    )
    {
    MapInfoType* minfop = &this->MapInfo[key.ObjectType][key.ArrayId];
    vtkIdTypeArray* iarr = vtkIdTypeArray::New();
    arr = iarr;
    arr->SetName( minfop->Name.c_str() );
    arr->SetNumberOfComponents( 1 );
    switch ( key.ObjectType )
      {
    case vtkExodusIIReader::NODE_MAP:
      arr->SetNumberOfTuples( this->ModelParameters.num_nodes );
      break;
    case vtkExodusIIReader::EDGE_MAP:
      arr->SetNumberOfTuples( this->ModelParameters.num_edge );
      break;
    case vtkExodusIIReader::FACE_MAP:
      arr->SetNumberOfTuples( this->ModelParameters.num_face );
      break;
    case vtkExodusIIReader::ELEM_MAP:
      arr->SetNumberOfTuples( this->ModelParameters.num_elem );
      break;
      }
#ifdef VTK_USE_64BIT_IDS
      {
      vtkstd::vector<int> tmpMap( arr->GetNumberOfTuples() );
      if ( ex_get_num_map( exoid, key.ObjectType, minfop->Id, &tmpMap[0] ) < 0 )
        {
        vtkErrorMacro( "Could not read map \"" << minfop->Name.c_str() << "\" (" << minfop->Id << ") from disk." );
        arr->Delete();
        arr = 0;
        return 0;
        }
      for ( vtkIdType i = 0; i < arr->GetNumberOfTuples(); ++i )
        {
        iarr->SetValue( i, tmpMap[i] );
        }
      }
#else
    if ( ex_get_num_map( exoid, key.ObjectType, minfop->Id, (int*)arr->GetVoidPointer( 0 ) ) < 0 )
      {
      vtkErrorMacro( "Could not read nodal map variable " << minfop->Name.c_str() << "." );
      arr->Delete();
      arr = 0;
      }
#endif // VTK_USE_64BIT_IDS
    }
  else if ( key.ObjectType == vtkExodusIIReader::GLOBAL_ELEMENT_ID )
    { // subset the ELEMENT_ID array choosing only entries for blocks that have Status ON
    vtkIdTypeArray* src = vtkIdTypeArray::SafeDownCast(
      this->GetCacheOrRead( vtkExodusIICacheKey( -1, vtkExodusIIReader::ELEMENT_ID, 0, 0 ) ) );
    if ( ! src )
      {
      arr = 0;
      return 0;
      }
    vtkIdTypeArray* iarr = vtkIdTypeArray::New();
    iarr->SetName( vtkExodusIIReader::GetGlobalElementIdArrayName() );
    iarr->SetNumberOfComponents( 1 );
    iarr->SetNumberOfTuples( this->NumberOfCells );
    vtkIdType* gloIds = iarr->GetPointer( 0 );
    vtkIdType* srcIds = src->GetPointer( 0 );
    memset( (void*)gloIds, 0, sizeof(vtkIdType) * this->NumberOfCells );

    vtkstd::vector<BlockInfoType>::iterator bi;
    for (
      bi = this->BlockInfo[vtkExodusIIReader::ELEM_BLOCK].begin();
      bi != this->BlockInfo[vtkExodusIIReader::ELEM_BLOCK].end();
      ++bi )
      {
      if ( ! bi->Status )
        continue;

      vtkIdType x;
      for ( x = 0; x < bi->Size; ++x )
        {
        gloIds[x + bi->GridOffset] = srcIds[x + bi->FileOffset - 1];
        }
      }
    arr = iarr;
    }
  else if ( key.ObjectType == vtkExodusIIReader::GLOBAL_NODE_ID )
    { // subset the NODE_ID array choosing only entries for nodes in output grid (using PointMap)
    vtkIdTypeArray* src = vtkIdTypeArray::SafeDownCast(
      this->GetCacheOrRead( vtkExodusIICacheKey( -1, vtkExodusIIReader::NODE_ID, 0, 0 ) ) );
    if ( ! src )
      {
      arr = 0;
      return 0;
      }     
    vtkIdTypeArray* iarr = vtkIdTypeArray::New();
    iarr->SetName( vtkExodusIIReader::GetGlobalNodeIdArrayName() );
    iarr->SetNumberOfComponents( 1 );
    iarr->SetNumberOfTuples( this->NextSqueezePoint );
    vtkIdType* gloIds = iarr->GetPointer( 0 );
    vtkIdType* srcIds = src->GetPointer( 0 );
    vtkIdType pt;
    for ( pt = 0; pt < this->ModelParameters.num_nodes; ++pt )
      {
      vtkIdType x = this->PointMap[pt];
      if ( x >= 0 )
        {
        gloIds[x] = srcIds[pt];
        }
      }
    arr = iarr;
    }
  else if (
    key.ObjectType == vtkExodusIIReader::ELEMENT_ID ||
    key.ObjectType == vtkExodusIIReader::NODE_ID
    )
    {
    vtkIdTypeArray* iarr;
    int nMaps;
    vtkIdType mapSize;
    vtkExodusIICacheKey ktmp;
    vtkExodusIIGetMapFunc getMapFunc;
    if ( key.ObjectType == vtkExodusIIReader::ELEMENT_ID )
      {
      nMaps = this->ModelParameters.num_elem_maps;
      mapSize = this->ModelParameters.num_elem;
      ktmp = vtkExodusIICacheKey( -1, vtkExodusIIReader::ELEM_MAP, 0, 0 );
      getMapFunc = ex_get_elem_num_map;
      }
    else // ( key.ObjectType == vtkExodusIIReader::NODE_ID )
      {
      nMaps = this->ModelParameters.num_node_maps;
      mapSize = this->ModelParameters.num_nodes;
      ktmp = vtkExodusIICacheKey( -1, vtkExodusIIReader::NODE_MAP, 0, 0 );
      getMapFunc = ex_get_node_num_map;
      }
    // If there are no new-style maps, get the old-style map (which creates a default if nothing is stored on disk).
    if ( nMaps < 1 || ! (iarr = vtkIdTypeArray::SafeDownCast(this->GetCacheOrRead( ktmp ))) )
      {
      iarr = vtkIdTypeArray::New();
      iarr->SetNumberOfComponents( 1 );
      iarr->SetNumberOfTuples( mapSize );
      if ( mapSize )
        {
#ifdef VTK_USE_64BIT_IDS
        vtkstd::vector<int> tmpMap( iarr->GetNumberOfTuples() );
        if ( getMapFunc( exoid, &tmpMap[0] ) < 0 )
          {
          vtkErrorMacro( "Could not read old-style node or element map." );
          iarr->Delete();
          iarr = 0;
          }
        else
          {
          for ( vtkIdType i = 0; i < iarr->GetNumberOfTuples(); ++i )
            {
            iarr->SetValue( i, tmpMap[i] );
            }
          }
#else
        if ( getMapFunc( exoid, (int*)iarr->GetPointer( 0 ) ) < 0 )
          {
          vtkErrorMacro( "Could not read old-style node or element map." );
          iarr->Delete();
          iarr = 0;
          }
#endif // VTK_USE_64BIT_IDS
        }
      }
    else
      {
      // FastDelete will be called below (because we are assumed to have created the array with New()).
      // So we must reference the array one extra time here to account for the extra delete...
      iarr->Register( this );
      }
    arr = iarr;
    }
  else if ( key.ObjectType == vtkExodusIIReader::GLOBAL_CONN )
    {
    vtkErrorMacro(
      "Global connectivity is created in AssembleOutputConnectivity since it can't be cached\n"
      "with a single vtkDataArray. Who told you to call this routine to get it?"
      );
    }
  else if (
    key.ObjectType == vtkExodusIIReader::ELEM_BLOCK_ELEM_CONN ||
    key.ObjectType == vtkExodusIIReader::FACE_BLOCK_CONN ||
    key.ObjectType == vtkExodusIIReader::EDGE_BLOCK_CONN
  )
    {

    int ctypidx = this->GetConnTypeIndexFromConnType( key.ObjectType );
    int otypidx = conn_obj_idx_cvt[ctypidx];
    int otyp = obj_types[ otypidx ];
    BlockInfoType* binfop = (BlockInfoType*) this->GetObjectInfo( otypidx, key.ObjectId );

    vtkIntArray* iarr = vtkIntArray::New();
    iarr->SetNumberOfComponents( binfop->BdsPerEntry[0] );
    iarr->SetNumberOfTuples( binfop->Size );
    
    if ( ex_get_conn( exoid, otyp, binfop->Id, iarr->GetPointer(0), 0, 0 ) < 0 )
      {
      vtkErrorMacro( "Unable to read " << objtype_names[otypidx] << " " << binfop->Id << " (index " << key.ObjectId <<
        ") nodal connectivity." );
      iarr->Delete();
      iarr = 0;
      }

    vtkIdType c;
    int* ptr = iarr->GetPointer( 0 );
    if (
      binfop->CellType == VTK_QUADRATIC_HEXAHEDRON ||
      binfop->CellType == VTK_TRIQUADRATIC_HEXAHEDRON
      )
      {
      // Edge order for VTK is different than Exodus edge order.
      for ( c = 0; c < iarr->GetNumberOfTuples(); ++c )
        {
        int k;
        int itmp[4];

        for ( k = 0; k < 12; ++k, ++ptr)
          *ptr = *ptr - 1;

        for ( k = 0; k < 4; ++k, ++ptr)
          {
          itmp[k] = *ptr;
          *ptr = ptr[4] - 1;
          }

        for ( k = 0; k < 4; ++k, ++ptr )
          *ptr = itmp[k] - 1;

        if ( binfop->CellType == VTK_TRIQUADRATIC_HEXAHEDRON )
          {
          for ( k = 0; k < 4; ++k, ++ptr )
            *ptr = *ptr - 1;
          }
        }
      ptr += binfop->BdsPerEntry[0] - binfop->PointsPerCell;
      }
    else
      {
      for ( c = 0; c <= iarr->GetMaxId(); ++c, ++ptr )
        {
        *ptr = *ptr - 1;
        }
      }

    arr = iarr;
    }
  else if ( key.ObjectType == vtkExodusIIReader::ELEM_BLOCK_FACE_CONN )
    {
    // FIXME: Call ex_get_conn with non-NULL face conn pointer
    // This won't be needed until the Exodus reader outputs multiblock data with vtkGenericDataSet blocks for higher order meshes.
    arr = 0;
    }
  else if ( key.ObjectType == vtkExodusIIReader::ELEM_BLOCK_EDGE_CONN )
    {
    // FIXME: Call ex_get_conn with non-NULL edge conn pointer
    // This won't be needed until the Exodus reader outputs multiblock data with vtkGenericDataSet blocks for higher order meshes.
    arr = 0;
    }
  else if (
    key.ObjectType == vtkExodusIIReader::NODE_SET_CONN ||
    key.ObjectType == vtkExodusIIReader::ELEM_SET_CONN
  )
    {
    int otyp = this->GetSetTypeFromSetConnType( key.ObjectType );
    int otypidx = this->GetObjectTypeIndexFromObjectType( otyp );
    SetInfoType* sinfop = &this->SetInfo[otyp][key.ObjectId];
    vtkIntArray* iarr = vtkIntArray::New();
    iarr->SetNumberOfComponents( 1 );
    iarr->SetNumberOfTuples( sinfop->Size );
    int* iptr = iarr->GetPointer( 0 );
    
    if ( ex_get_set( exoid, otyp, sinfop->Id, iptr, 0 ) < 0 )
      {
      vtkErrorMacro( "Unable to read " << objtype_names[otypidx] << " " << sinfop->Id << " (index " << key.ObjectId <<
        ") nodal connectivity." );
      iarr->Delete();
      iarr = 0;
      }

    vtkIdType id;
    for ( id = 0; id < sinfop->Size; ++id, ++iptr )
      { // VTK uses 0-based indexing, unlike Exodus:
      --(*iptr);
      }

    arr = iarr;
    }
  else if ( key.ObjectType == vtkExodusIIReader::EDGE_SET_CONN || key.ObjectType == vtkExodusIIReader::FACE_SET_CONN )
    {
    int otyp = this->GetSetTypeFromSetConnType( key.ObjectType );
    int otypidx = this->GetObjectTypeIndexFromObjectType( otyp );
    SetInfoType* sinfop = &this->SetInfo[otyp][key.ObjectId];
    vtkIntArray* iarr = vtkIntArray::New();
    iarr->SetNumberOfComponents( 2 );
    iarr->SetNumberOfTuples( sinfop->Size );
    vtkstd::vector<int> tmpOrient; // hold the edge/face orientation information until we can interleave it.
    tmpOrient.resize( sinfop->Size );
    
    if ( ex_get_set( exoid, otyp, sinfop->Id, iarr->GetPointer(0), &tmpOrient[0] ) < 0 )
      {
      vtkErrorMacro( "Unable to read " << objtype_names[otypidx] << " " << sinfop->Id << " (index " << key.ObjectId <<
        ") nodal connectivity." );
      iarr->Delete();
      iarr = 0;
      return 0;
      }

    int* iap = iarr->GetPointer( 0 );
    vtkIdType c;
    for ( c = sinfop->Size - 1; c >= 0; --c )
      {
      iap[2*c] = iap[c] - 1; // VTK uses 0-based indexing
      iap[2*c + 1] = tmpOrient[c];
      }

    arr = iarr;
    }
  else if ( key.ObjectType == vtkExodusIIReader::SIDE_SET_CONN )
    {
    // Stick all of side_set_node_list and side_set_node_count and side_set_nodes_per_side in one array
    // let InsertSetSides() figure it all out. Except for 0-based indexing
    SetInfoType* sinfop = &this->SetInfo[vtkExodusIIReader::SIDE_SET][key.ObjectId];
    int ssnllen; // side set node list length
    if ( ex_get_side_set_node_list_len( exoid, sinfop->Id, &ssnllen ) < 0 )
      {
      vtkErrorMacro( "Unable to fetch side set \"" << sinfop->Name.c_str() << "\" (" << sinfop->Id << ") node list length" );
      arr = 0;
      return 0;
      }
    vtkIntArray* iarr = vtkIntArray::New();
    vtkIdType ilen = ssnllen + sinfop->Size;
    iarr->SetNumberOfComponents( 1 );
    iarr->SetNumberOfTuples( ilen );
    int* dat = iarr->GetPointer( 0 );
    if ( ex_get_side_set_node_list( exoid, sinfop->Id, dat, dat + sinfop->Size ) < 0 )
      {
      vtkErrorMacro( "Unable to fetch side set \"" << sinfop->Name.c_str() << "\" (" << sinfop->Id << ") node list" );
      iarr->Delete();
      arr = 0;
      return 0;
      }
    while ( ilen > sinfop->Size )
      { // move to 0-based indexing on nodes, don't touch nodes/side counts at head of array
      --dat[--ilen];
      }
    arr = iarr;
    }
  else if ( key.ObjectType == vtkExodusIIReader::NODAL_COORDS )
    {
    // read node coords
    vtkDataArray* displ = 0;
    if ( this->ApplyDisplacements && key.Time >= 0 )
      {
      displ = this->FindDisplacementVectors( key.Time );
      }

    vtkstd::vector<double> coordTmp;
    vtkDoubleArray* darr = vtkDoubleArray::New();
    arr = darr;
    arr->SetNumberOfComponents( 3 );
    arr->SetNumberOfTuples( this->ModelParameters.num_nodes );
    int dim = this->ModelParameters.num_dim;
    int c;
    vtkIdType t;
    double* xc = 0;
    double* yc = 0;
    double* zc = 0;
    coordTmp.resize( this->ModelParameters.num_nodes );
    for ( c = 0; c < dim; ++c )
      {
      switch ( c )
        {
      case 0:
        xc = &coordTmp[0];
        break;
      case 1:
        yc = xc;
        xc = 0;
        break;
      case 2:
        zc = yc;
        yc = 0;
        break;
      default:
        vtkErrorMacro( "Bad coordinate index " << c << " when reading point coordinates." );
        xc = yc = zc = 0;
        }
      if ( ex_get_coord( exoid, xc, yc, zc ) < 0 )
        {
        vtkErrorMacro( "Unable to read node coordinates for index " << c << "." );
        arr->Delete();
        arr = 0;
        break;
        }
      double* cptr = darr->GetPointer( c );
      for ( t = 0; t < this->ModelParameters.num_nodes; ++t )
        {
        *cptr = coordTmp[t];
        cptr += 3;
        }
      }
    if ( displ )
      {
      double* coords = darr->GetPointer( 0 );
      for ( vtkIdType idx = 0; idx < displ->GetNumberOfTuples(); ++idx )
        {
        double* dispVal = displ->GetTuple3( idx );
        for ( c = 0; c < 3; ++c )
          coords[c] += dispVal[c] * this->DisplacementMagnitude;

        coords += 3;
        }
      }
    }
  else if ( key.ObjectType == vtkExodusIIReader::GLOBAL_OBJECT_ID )
    {
    arr = vtkIntArray::New();
    arr->SetName( this->GetObjectIdArrayName() );
    arr->SetNumberOfComponents( 1 );
    arr->SetNumberOfTuples( this->NumberOfCells );

    int conntypidx;
    for ( conntypidx = 0; conntypidx < num_conn_types; ++conntypidx )
      {
      int otypidx = conn_obj_idx_cvt[conntypidx];
      int obj;
      int numObj = this->GetNumberOfObjectsAtTypeIndex( otypidx );
      BlockSetInfoType* bsinfop;
      for ( obj = 0; obj < numObj; ++obj )
        {
        bsinfop = (BlockSetInfoType*) this->GetObjectInfo( otypidx, obj );
        if ( ! bsinfop->Status )
          continue;

        vtkIdType c;
        for ( c = 0; c < bsinfop->Size; ++c )
          {
          arr->SetTuple1( c + bsinfop->GridOffset, bsinfop->Id );
          }
        }
      }
    }
  else if (
    key.ObjectType == vtkExodusIIReader::ELEM_BLOCK_ATTRIB ||
    key.ObjectType == vtkExodusIIReader::FACE_BLOCK_ATTRIB ||
    key.ObjectType == vtkExodusIIReader::EDGE_BLOCK_ATTRIB
    )
    {
    BlockInfoType* binfop = &this->BlockInfo[key.ObjectType][key.ObjectId];
    vtkDoubleArray* darr = vtkDoubleArray::New();
    arr = darr;
    darr->SetName( binfop->AttributeNames[key.ArrayId].c_str() );
    darr->SetNumberOfComponents( 1 );
    darr->SetNumberOfTuples( binfop->Size );
    if ( ex_get_one_attr( exoid, key.ObjectType, key.ObjectId, key.ArrayId, darr->GetVoidPointer( 0 ) ) < 0 )
      { // NB: The error message references the file-order object id, not the numerically sorted index presented to users.
      vtkErrorMacro( "Unable to read attribute " << key.ArrayId
        << " for object " << key.ObjectId << " of type " << key.ObjectType << "." );
      arr->Delete();
      arr = 0;
      }
    }
  else
    {
    vtkWarningMacro( "You requested an array for objects of type " << key.ObjectType <<
      " which I know nothing about" );
    arr = 0;
    }

  // Even if the array is larger than the allowable cache size, it will keep the most recent insertion.
  // So, we delete our reference knowing that the Cache will keep the object "alive" until whatever
  // called GetCacheOrRead() references the array. But, once you get an array from GetCacheOrRead(),
  // you better start running!
  if ( arr )
    {
    this->Cache->Insert( key, arr );
    arr->FastDelete();
    }
  return arr;
}

int vtkExodusIIReaderPrivate::GetConnTypeIndexFromConnType( int ctyp )
{
  int i;
  for ( i = 0; i < num_conn_types; ++i )
    {
    if ( conn_types[i] == ctyp )
      {
      return i;
      }
    }
  return -1;
}

int vtkExodusIIReaderPrivate::GetObjectTypeIndexFromObjectType( int otyp )
{
  int i;
  for ( i = 0; i < num_obj_types; ++i )
    {
    if ( obj_types[i] == otyp )
      {
      return i;
      }
    }
  return -1;
}

int vtkExodusIIReaderPrivate::GetNumberOfObjectsAtTypeIndex( int typeIndex )
{
  if ( typeIndex < 0 )
    {
    return 0;
    }
  else if ( typeIndex < 3 )
    {
    return (int) this->BlockInfo[obj_types[typeIndex]].size();
    }
  else if ( typeIndex < 8 )
    {
    return (int) this->SetInfo[obj_types[typeIndex]].size();
    }
  else if ( typeIndex < 12 )
    {
    return (int) this->MapInfo[obj_types[typeIndex]].size();
    }
  return 0;
}

vtkExodusIIReaderPrivate::ObjectInfoType* vtkExodusIIReaderPrivate::GetObjectInfo( int typeIndex, int objectIndex )
{
  if ( typeIndex < 0 )
    {
    return 0;
    }
  else if ( typeIndex < 3 )
    {
    return &this->BlockInfo[obj_types[typeIndex]][objectIndex];
    }
  else if ( typeIndex < 8 )
    {
    return &this->SetInfo[obj_types[typeIndex]][objectIndex];
    }
  else if ( typeIndex < 12 )
    {
    return &this->MapInfo[obj_types[typeIndex]][objectIndex];
    }
  return 0;
}

vtkExodusIIReaderPrivate::ObjectInfoType* vtkExodusIIReaderPrivate::GetSortedObjectInfo( int otyp, int k )
{
  int i = this->GetObjectTypeIndexFromObjectType( otyp );
  if ( i < 0 )
    {
    vtkWarningMacro( "Could not find collection of objects with type " << otyp << "." );
    return 0;
    }
  int N = this->GetNumberOfObjectsAtTypeIndex( i );
  if ( k < 0 || k >= N )
    {
    vtkWarningMacro( "You requested object " << k << " in a collection of only " << N << " objects." );
    return 0;
    }
  return this->GetObjectInfo( i, this->SortedObjectIndices[otyp][k] );
}

int vtkExodusIIReaderPrivate::GetBlockIndexFromFileGlobalId( int otyp, int refId )
{
  vtkstd::vector<BlockInfoType>::iterator bi;
  int i = 0;
  for ( bi = this->BlockInfo[otyp].begin(); bi != this->BlockInfo[otyp].end(); ++bi, ++i )
    {
    if ( refId >= bi->FileOffset && refId <= bi->FileOffset + bi->Size )
      return i;
    }
  return -1;
}

vtkExodusIIReaderPrivate::BlockInfoType* vtkExodusIIReaderPrivate::GetBlockFromFileGlobalId( int otyp, int refId )
{
  int blk = this->GetBlockIndexFromFileGlobalId( otyp, refId );
  if ( blk >= 0 )
    {
    return &this->BlockInfo[otyp][blk];
    }
  return 0;
}

vtkIdType vtkExodusIIReaderPrivate::GetSqueezePointId( int i )
{
  vtkIdType* x = &this->PointMap[i];
  if ( *x < 0 )
    {
    *x = this->NextSqueezePoint++;
    }
  return *x;
}

void vtkExodusIIReaderPrivate::DetermineVtkCellType( BlockInfoType& binfo )
{
  vtkStdString elemType( vtksys::SystemTools::UpperCase( binfo.TypeName ) );

  // Check for quadratic elements
  if ((elemType.substr(0,3) == "TRI") &&           (binfo.BdsPerEntry[0] == 6))
    { binfo.CellType=VTK_QUADRATIC_TRIANGLE;       binfo.PointsPerCell = 6; }
  else if ((elemType.substr(0,3) == "SHE") &&      (binfo.BdsPerEntry[0] == 8))
    { binfo.CellType=VTK_QUADRATIC_QUAD;           binfo.PointsPerCell = 8; }
  else if ((elemType.substr(0,3) == "SHE") &&      (binfo.BdsPerEntry[0] == 9))
    { binfo.CellType=VTK_QUADRATIC_QUAD;           binfo.PointsPerCell = 8; }
  else if ((elemType.substr(0,3) == "TET") &&      (binfo.BdsPerEntry[0] == 10))
    { binfo.CellType=VTK_QUADRATIC_TETRA;          binfo.PointsPerCell = 10; }
  else if ((elemType.substr(0,3) == "TET") &&      (binfo.BdsPerEntry[0] == 11))
    { binfo.CellType=VTK_QUADRATIC_TETRA;          binfo.PointsPerCell = 10; }
  else if ((elemType.substr(0,3) == "HEX") &&      (binfo.BdsPerEntry[0] == 20))
    { binfo.CellType=VTK_QUADRATIC_HEXAHEDRON;     binfo.PointsPerCell = 20; }
  else if ((elemType.substr(0,3) == "HEX") &&      (binfo.BdsPerEntry[0] == 21))
    { binfo.CellType=VTK_QUADRATIC_HEXAHEDRON;     binfo.PointsPerCell = 20; }
  else if ((elemType.substr(0,3) == "HEX") &&      (binfo.BdsPerEntry[0] == 27))
    { binfo.CellType=VTK_TRIQUADRATIC_HEXAHEDRON;  binfo.PointsPerCell = 27; }
  else if ((elemType.substr(0,3) == "QUA") &&      (binfo.BdsPerEntry[0] == 8))
    { binfo.CellType=VTK_QUADRATIC_QUAD;           binfo.PointsPerCell = 8; }
  else if ((elemType.substr(0,3) == "QUA") &&      (binfo.BdsPerEntry[0] == 9))
    { binfo.CellType=VTK_BIQUADRATIC_QUAD;         binfo.PointsPerCell = 9; }
  else if ((elemType.substr(0,3) == "TRU") &&      (binfo.BdsPerEntry[0] == 3))
    { binfo.CellType=VTK_QUADRATIC_EDGE;           binfo.PointsPerCell = 3; }
  else if ((elemType.substr(0,3) == "BEA") &&      (binfo.BdsPerEntry[0] == 3))
    { binfo.CellType=VTK_QUADRATIC_EDGE;           binfo.PointsPerCell = 3; }
  else if ((elemType.substr(0,3) == "BAR") &&      (binfo.BdsPerEntry[0] == 3))
    { binfo.CellType=VTK_QUADRATIC_EDGE;           binfo.PointsPerCell = 3; }
  else if ((elemType.substr(0,3) == "EDG") &&      (binfo.BdsPerEntry[0] == 3))
    { binfo.CellType=VTK_QUADRATIC_EDGE;           binfo.PointsPerCell = 3; }

  // Check for regular elements
  else if ((elemType.substr(0,3) == "CIR")) { binfo.CellType = VTK_VERTEX;     binfo.PointsPerCell = 1; }
  else if ((elemType.substr(0,3) == "SPH")) { binfo.CellType = VTK_VERTEX;     binfo.PointsPerCell = 1; }
  else if ((elemType.substr(0,3) == "BAR")) { binfo.CellType = VTK_LINE;       binfo.PointsPerCell = 2; }
  else if ((elemType.substr(0,3) == "TRU")) { binfo.CellType = VTK_LINE;       binfo.PointsPerCell = 2; }
  else if ((elemType.substr(0,3) == "BEA")) { binfo.CellType = VTK_LINE;       binfo.PointsPerCell = 2; }
  else if ((elemType.substr(0,3) == "EDG")) { binfo.CellType = VTK_LINE;       binfo.PointsPerCell = 2; }
  else if ((elemType.substr(0,3) == "TRI")) { binfo.CellType = VTK_TRIANGLE;   binfo.PointsPerCell = 3; }
  else if ((elemType.substr(0,3) == "QUA")) { binfo.CellType = VTK_QUAD;       binfo.PointsPerCell = 4; }
  else if ((elemType.substr(0,3) == "TET")) { binfo.CellType = VTK_TETRA;      binfo.PointsPerCell = 4; }
  else if ((elemType.substr(0,3) == "PYR")) { binfo.CellType = VTK_PYRAMID;    binfo.PointsPerCell = 5; }
  else if ((elemType.substr(0,3) == "WED")) { binfo.CellType = VTK_WEDGE;      binfo.PointsPerCell = 6; }
  else if ((elemType.substr(0,3) == "HEX")) { binfo.CellType = VTK_HEXAHEDRON; binfo.PointsPerCell = 8; }
  else if ((elemType.substr(0,3) == "SHE") && (binfo.BdsPerEntry[0] == 3))
    { binfo.CellType = VTK_TRIANGLE;           binfo.PointsPerCell = 3; }
  else if ((elemType.substr(0,3) == "SHE") && (binfo.BdsPerEntry[0] == 4))
    { binfo.CellType = VTK_QUAD;               binfo.PointsPerCell = 4; }
  else if ((elemType.substr(0,8) == "STRAIGHT") && (binfo.BdsPerEntry[0] == 2 ))
    { binfo.CellType = VTK_LINE;                    binfo.PointsPerCell = 2; }
  else if ((elemType.substr(0,8) == "NULL") && (binfo.Size == 0))
    {
    (void)binfo; // silently ignore empty element blocks
    }
  else
    {
    vtkErrorMacro("Unsupported element type: " << elemType.c_str());
    }

  //cell types not currently handled
  //quadratic wedge - 15,16 nodes
  //quadratic pyramid - 13 nodes
}

vtkExodusIIReaderPrivate::ArrayInfoType* vtkExodusIIReaderPrivate::FindArrayInfoByName( int otyp, const char* name )
{
  vtkstd::vector<ArrayInfoType>::iterator ai;
  for ( ai = this->ArrayInfo[otyp].begin(); ai != this->ArrayInfo[otyp].end(); ++ai )
    {
    if ( ai->Name == name )
      return &(*ai);
    }
  return 0;
}

int vtkExodusIIReaderPrivate::IsObjectTypeBlock( int otyp )
{
  return (otyp == vtkExodusIIReader::ELEM_BLOCK || otyp == vtkExodusIIReader::EDGE_BLOCK || otyp == vtkExodusIIReader::FACE_BLOCK);
}

int vtkExodusIIReaderPrivate::IsObjectTypeSet( int otyp )
{
  return (otyp == vtkExodusIIReader::ELEM_SET || otyp == vtkExodusIIReader::EDGE_SET || otyp == vtkExodusIIReader::FACE_SET || otyp == vtkExodusIIReader::NODE_SET || otyp == vtkExodusIIReader::SIDE_SET);
}

int vtkExodusIIReaderPrivate::IsObjectTypeMap( int otyp )
{
  return (otyp == vtkExodusIIReader::ELEM_MAP || otyp == vtkExodusIIReader::EDGE_MAP || otyp == vtkExodusIIReader::FACE_MAP || otyp == vtkExodusIIReader::NODE_MAP);
}

int vtkExodusIIReaderPrivate::GetObjectTypeFromMapType( int mtyp )
{
  switch (mtyp)
    {
  case vtkExodusIIReader::ELEM_MAP:
    return vtkExodusIIReader::ELEM_BLOCK;
  case vtkExodusIIReader::FACE_MAP:
    return vtkExodusIIReader::FACE_BLOCK;
  case vtkExodusIIReader::EDGE_MAP:
    return vtkExodusIIReader::EDGE_BLOCK;
  case vtkExodusIIReader::NODE_MAP:
    return vtkExodusIIReader::NODAL;
    }
  return -1;
}

int vtkExodusIIReaderPrivate::GetMapTypeFromObjectType( int otyp )
{
  switch (otyp)
    {
  case vtkExodusIIReader::ELEM_BLOCK:
    return vtkExodusIIReader::ELEM_MAP;
  case vtkExodusIIReader::FACE_BLOCK:
    return vtkExodusIIReader::FACE_MAP;
  case vtkExodusIIReader::EDGE_BLOCK:
    return vtkExodusIIReader::EDGE_MAP;
  case vtkExodusIIReader::NODAL:
    return vtkExodusIIReader::NODE_MAP;
    }
  return -1;
}

int vtkExodusIIReaderPrivate::GetSetTypeFromSetConnType( int sctyp )
{
  switch ( sctyp )
    {
  case vtkExodusIIReader::NODE_SET_CONN:
    return vtkExodusIIReader::NODE_SET;
  case vtkExodusIIReader::EDGE_SET_CONN:
    return vtkExodusIIReader::EDGE_SET;
  case vtkExodusIIReader::FACE_SET_CONN:
    return vtkExodusIIReader::FACE_SET;
  case vtkExodusIIReader::SIDE_SET_CONN:
    return vtkExodusIIReader::SIDE_SET;
  case vtkExodusIIReader::ELEM_SET_CONN:
    return vtkExodusIIReader::ELEM_SET;
    }
  return -1;
}

int vtkExodusIIReaderPrivate::GetBlockConnTypeFromBlockType( int btyp )
{
  switch ( btyp )
    {
  case vtkExodusIIReader::EDGE_BLOCK:
    return vtkExodusIIReader::EDGE_BLOCK_CONN;
  case vtkExodusIIReader::FACE_BLOCK:
    return vtkExodusIIReader::FACE_BLOCK_CONN;
  case vtkExodusIIReader::ELEM_BLOCK:
    return vtkExodusIIReader::ELEM_BLOCK_ELEM_CONN;
    }
  return -1;
}

void vtkExodusIIReaderPrivate::RemoveBeginningAndTrailingSpaces( int len, char **names )
{
  int i, j;

  for (i=0; i<len; i++)
    {
    char *c = names[i];
    int nmlen = strlen(c);

    char *cbegin = c;
    char *cend = c + nmlen - 1;

    // remove spaces or non-printing character from start and end

    for (j=0; j<nmlen; j++)
      {
      if (!isgraph(*cbegin)) cbegin++;
      else break;
      }

    for (j=0; j<nmlen; j++)
      {
      if (!isgraph(*cend)) cend--;
      else break;
      }

    if (cend < cbegin)
      {
      sprintf(names[i], "null_%d", i);
      continue;
      }

    int newlen = cend - cbegin + 1;

    if (newlen < nmlen)
      {
      for (j=0; j<newlen; j++)
        {
        *c++ = *cbegin++;
        }
      *c = '\0';
      }
    }
}

// Normally, this would be below with all the other vtkExodusIIReader member definitions,
// but the Tcl PrintSelf test script is really lame.
void vtkExodusIIReader::PrintSelf( ostream& os, vtkIndent indent )
{
  this->Superclass::PrintSelf( os, indent );
  os << indent << "FileName: " << ( this->FileName ? this->FileName : "(null)" ) << "\n";
  os << indent << "XMLFileName: " << ( this->XMLFileName ? this->XMLFileName : "(null)" ) << "\n";
  os << indent << "DisplayType: " << this->DisplayType << "\n";
  os << indent << "TimeStep: " << this->TimeStep << "\n";
  os << indent << "ExodusModelMetadata: " << (this->ExodusModelMetadata ? "ON" : "OFF" ) << "\n";
  os << indent << "PackExodusModelOntoOutput: " << (this->PackExodusModelOntoOutput ? "ON" : "OFF" ) << "\n";
  os << indent << "ExodusModel: " << this->ExodusModel << "\n";
  if ( this->Metadata )
    {
    os << indent << "Metadata:\n";
    this->Metadata->PrintData( os, indent.GetNextIndent() );
    }
  else
    {
    os << indent << "Metadata: (null)\n";
    }
}


void vtkExodusIIReaderPrivate::PrintData( ostream& os, vtkIndent indent )
{
  //this->Superclass::Print Self( os, indent );
  os << indent << "Exoid: " << this->Exoid << "\n";
  os << indent << "AppWordSize: " << this->AppWordSize << "\n";
  os << indent << "DiskWordSize: " << this->DiskWordSize << "\n";
  os << indent << "ExodusVersion: " << this->ExodusVersion << "\n";
  os << indent << "ModelParameters:\n";

  vtkIndent inden2 = indent.GetNextIndent();
  os << inden2 << "Title: " << this->ModelParameters.title << "\n";
  os << inden2 << "Dimension: " << this->ModelParameters.num_dim << "\n";
  os << inden2 << "Nodes: " << this->ModelParameters.num_nodes << "\n";
  os << inden2 << "Edges: " << this->ModelParameters.num_edge << "\n";
  os << inden2 << "Faces: " << this->ModelParameters.num_face << "\n";
  os << inden2 << "Elements: " << this->ModelParameters.num_elem << "\n";
  os << inden2 << "Edge Blocks: " << this->ModelParameters.num_edge_blk << "\n";
  os << inden2 << "Face Blocks: " << this->ModelParameters.num_face_blk << "\n";
  os << inden2 << "Element Blocks: " << this->ModelParameters.num_elem_blk << "\n";
  os << inden2 << "Node Sets: " << this->ModelParameters.num_node_sets << "\n";
  os << inden2 << "Edge Sets: " << this->ModelParameters.num_edge_sets << "\n";
  os << inden2 << "Face Sets: " << this->ModelParameters.num_face_sets << "\n";
  os << inden2 << "Side Sets: " << this->ModelParameters.num_side_sets << "\n";
  os << inden2 << "Element Sets: " << this->ModelParameters.num_elem_sets << "\n";
  os << inden2 << "Node Maps: " << this->ModelParameters.num_node_maps << "\n";
  os << inden2 << "Edge Maps: " << this->ModelParameters.num_edge_maps << "\n";
  os << inden2 << "Face Maps: " << this->ModelParameters.num_face_maps << "\n";
  os << inden2 << "Element Maps: " << this->ModelParameters.num_elem_maps << "\n";

  os << indent << "Time steps (" << this->Times.size() << "):";
  int i;
  for ( i = 0; i < (int)this->Times.size(); ++i )
    {
    os << " " << this->Times[i];
    }
  os << "\n";
  os << indent << "TimeStep: " << this->TimeStep << "\n";

  // Print nodal variables
  if ( this->ArrayInfo[ vtkExodusIIReader::NODAL ].size() > 0 )
    {
    os << indent << "Nodal Arrays:\n";
    vtkstd::vector<ArrayInfoType>::iterator ai;
    for ( ai = this->ArrayInfo[ vtkExodusIIReader::NODAL ].begin(); ai != this->ArrayInfo[ vtkExodusIIReader::NODAL ].end(); ++ai )
      {
      printArray( os, indent, vtkExodusIIReader::NODAL, *ai );
      }
    }

  // Print blocks
  os << indent << "Blocks:\n";
  vtkstd::map<int,vtkstd::vector<BlockInfoType> >::iterator bti;
  for ( bti = this->BlockInfo.begin(); bti != this->BlockInfo.end(); ++bti )
    {
    vtkstd::vector<BlockInfoType>::iterator bi;
    for ( bi = bti->second.begin(); bi != bti->second.end(); ++bi )
      {
      printBlock( os, indent.GetNextIndent(), bti->first, *bi );
      }
    if ( this->ArrayInfo[ bti->first ].size() > 0 )
      {
      os << indent << "    Results variables:\n";
      vtkstd::vector<ArrayInfoType>::iterator ai;
      for ( ai = this->ArrayInfo[ bti->first ].begin(); ai != this->ArrayInfo[ bti->first ].end(); ++ai )
        {
        printArray( os, indent.GetNextIndent(), bti->first, *ai );
        }
      }
    }

  // Print sets
  os << indent << "Sets:\n";
  vtkstd::map<int,vtkstd::vector<SetInfoType> >::iterator sti;
  for ( sti = this->SetInfo.begin(); sti != this->SetInfo.end(); ++sti )
    {
    vtkstd::vector<SetInfoType>::iterator si;
    for ( si = sti->second.begin(); si != sti->second.end(); ++si )
      {
      printSet( os, indent.GetNextIndent(), sti->first, *si );
      }
    if ( this->ArrayInfo[ sti->first ].size() > 0 )
      {
      os << indent << "    Results variables:\n";
      vtkstd::vector<ArrayInfoType>::iterator ai;
      for ( ai = this->ArrayInfo[ sti->first ].begin(); ai != this->ArrayInfo[ sti->first ].end(); ++ai )
        {
        printArray( os, indent.GetNextIndent(), sti->first, *ai );
        }
      }
    }

  // Print maps
  os << indent << "Maps:\n";
  vtkstd::map<int,vtkstd::vector<MapInfoType> >::iterator mti;
  for ( mti = this->MapInfo.begin(); mti != this->MapInfo.end(); ++mti )
    {
    vtkstd::vector<MapInfoType>::iterator mi;
    for ( mi = mti->second.begin(); mi != mti->second.end(); ++mi )
      {
      printMap( os, indent.GetNextIndent(), mti->first, *mi );
      }
    }

  os << indent << "Array Cache:\n";
  this->Cache->PrintSelf( os, inden2 );

  os << indent << "Number of output cells: " << this->NumberOfCells << "\n";

  os << indent << "SqueezePoints: " << this->SqueezePoints << "\n";
  os << indent << "NextSqueezePoint: " << this->NextSqueezePoint << "\n";
  os << indent << "ApplyDisplacements: " << this->ApplyDisplacements << "\n";
  os << indent << "DisplacementMagnitude: " << this->DisplacementMagnitude << "\n";
  os << indent << "GenerateObjectIdArray: " << this->GenerateObjectIdArray << "\n";
}

int vtkExodusIIReaderPrivate::OpenFile( const char* filename )
{
  if ( ! filename || ! strlen( filename ) )
    {
    vtkErrorMacro( "Exodus filename pointer was NULL or pointed to an empty string." );
    return 0;
    }

  if ( this->Exoid >= 0 )
    {
    this->CloseFile();
    }

  this->Exoid = ex_open( filename, EX_READ,
    &this->AppWordSize, &this->DiskWordSize, &this->ExodusVersion );

  if ( this->Exoid <= 0 )
    {
    vtkErrorMacro( "Unable to open \"" << filename << "\" for reading" );
    return 0;
    }

  return 1;
}

int vtkExodusIIReaderPrivate::CloseFile()
{
  if ( this->Exoid >= 0 )
    {
    VTK_EXO_FUNC( ex_close( this->Exoid ), "Could not close an open file (" << this->Exoid << ")" );
    this->Exoid = -1;
    }
  return 0;
}

int vtkExodusIIReaderPrivate::RequestInformation()
{
  int exoid = this->Exoid;
  int itmp[5];
  int* ids;
  int nids;
  int obj;
  int i, j;
  int num_timesteps;
  char** obj_names;
  char** obj_typenames = 0;
  char** var_names = 0;
  int have_var_names;
  int num_vars = 0; /* number of variables per object */
  //int num_entries; /* number of values per variable per object */

  this->Modified(); // Update MTime so that it will be newer than parent's FileNameMTime

  VTK_EXO_FUNC( ex_get_init_ext( exoid, &this->ModelParameters ),
    "Unable to read database parameters." );

  VTK_EXO_FUNC( ex_inquire( exoid, EX_INQ_TIME,       itmp, 0, 0 ), "Inquire for EX_INQ_TIME failed" );
  num_timesteps = itmp[0];

  vtkstd::vector<BlockInfoType> bitBlank;
  vtkstd::vector<SetInfoType> sitBlank;
  vtkstd::vector<MapInfoType> mitBlank;
  vtkstd::vector<ArrayInfoType> aitBlank;

  this->Times.clear();
  if ( num_timesteps > 0 )
    {
    this->Times.reserve( num_timesteps );
    this->Times.resize( num_timesteps );
    VTK_EXO_FUNC( ex_get_all_times( this->Exoid, &this->Times[0] ), "Could not retrieve time values." );
    }

  this->NumberOfCells = 0;
  for ( i = 0; i < num_obj_types; ++i )
    {
    vtkIdType blockEntryFileOffset = 1;
    vtkIdType setEntryFileOffset = 1;
    vtkIdType blockEntryGridOffset = 0;
    vtkIdType setEntryGridOffset = 0;

    vtkstd::map<int,int> sortedObjects;

    int* truth_tab = 0;
    have_var_names = 0;

    VTK_EXO_FUNC( ex_inquire( exoid, obj_sizes[i], &nids, 0, 0 ), "Object ID list size could not be determined." );

    if ( nids )
      {
      ids = (int*) malloc( nids * sizeof(int) );
      obj_names = (char**) malloc( nids * sizeof(char*) );
      for ( obj = 0; obj < nids; ++obj )
        obj_names[obj] = (char*) malloc( (MAX_STR_LENGTH + 1) * sizeof(char) );
      if ( OBJTYPE_IS_BLOCK(i) )
        {
        obj_typenames = (char**) malloc( nids * sizeof(char*) );
        for ( obj = 0; obj < nids; ++obj )
          {
          obj_typenames[obj] = (char*) malloc( (MAX_STR_LENGTH + 1) * sizeof(char) );
          obj_typenames[obj][0] = '\0';
          }
        }
      }
    else
      {
      ids = 0;
      obj_names = 0;
      obj_typenames = 0;
      }

    if ( nids == 0 && ! OBJTYPE_IS_MAP(i) )
      continue;

    if ( nids )
      {
      VTK_EXO_FUNC( ex_get_ids( exoid, obj_types[i], ids ), "Could not read object ids." );
      VTK_EXO_FUNC( ex_get_names( exoid, obj_types[i], obj_names ), "Could not read object names." );
      }

    BlockInfoType binfo;
    SetInfoType sinfo;
    MapInfoType minfo;
    if ( OBJTYPE_IS_BLOCK(i) )
      {
      this->BlockInfo[obj_types[i]] = bitBlank;
      this->BlockInfo[obj_types[i]].reserve( nids );
      }
    else if ( OBJTYPE_IS_SET(i) )
      {
      this->SetInfo[obj_types[i]] = sitBlank;
      this->SetInfo[obj_types[i]].reserve( nids );
      }
    else
      {
      this->MapInfo[obj_types[i]] = mitBlank;
      this->MapInfo[obj_types[i]].reserve( nids );
      }

    if ( (OBJTYPE_IS_BLOCK(i)) || (OBJTYPE_IS_SET(i)) ) {
      VTK_EXO_FUNC( ex_get_var_param( exoid, obj_typestr[i], &num_vars ), "Could not read number of variables." );

      if ( num_vars && num_timesteps > 0 ) {
        truth_tab = (int*) malloc( num_vars * nids * sizeof(int) );
        VTK_EXO_FUNC( ex_get_var_tab( exoid, obj_typestr[i], nids, num_vars, truth_tab ), "Could not read truth table." );

        var_names = (char**) malloc( num_vars * sizeof(char*) );
        for ( j = 0; j < num_vars; ++j )
          var_names[j] = (char*) malloc( (MAX_STR_LENGTH + 1) * sizeof(char) );

        VTK_EXO_FUNC( ex_get_var_names( exoid, obj_typestr[i], num_vars, var_names ), "Could not read variable names." );
        this->RemoveBeginningAndTrailingSpaces( num_vars, var_names );
        have_var_names = 1;
      }
    }

    if ( ! have_var_names )
      var_names = 0;

    for ( obj = 0; obj < nids; ++obj ) {

      if ( OBJTYPE_IS_BLOCK(i) ) {

        binfo.Name = obj_names[obj];
        binfo.Id = ids[obj];
        if ( obj_types[i] == vtkExodusIIReader::ELEM_BLOCK ) {
          VTK_EXO_FUNC( ex_get_block( exoid, obj_types[i], ids[obj], obj_typenames[obj],
              &binfo.Size, &binfo.BdsPerEntry[0], &binfo.BdsPerEntry[1], &binfo.BdsPerEntry[2], &binfo.AttributesPerEntry ),
            "Could not read block params." );
          binfo.Status = 1; // load element blocks by default
          binfo.TypeName = obj_typenames[obj];
        } else {
          VTK_EXO_FUNC( ex_get_block( exoid, obj_types[i], ids[obj], obj_typenames[obj],
              &binfo.Size, &binfo.BdsPerEntry[0], &binfo.BdsPerEntry[1], &binfo.BdsPerEntry[2], &binfo.AttributesPerEntry ),
            "Could not read block params." );
          binfo.Status = 0; // don't load edge/face blocks by default
          binfo.TypeName = obj_typenames[obj];
          binfo.BdsPerEntry[1] = binfo.BdsPerEntry[2] = 0;
        }
        //num_entries = binfo.Size;
        binfo.FileOffset = blockEntryFileOffset;
        blockEntryFileOffset += binfo.Size;
        if ( binfo.Status )
          {
          binfo.GridOffset = blockEntryGridOffset;
          blockEntryGridOffset += binfo.Size;
          this->NumberOfCells += binfo.Size;
          }
        else
          {
          binfo.GridOffset = -1;
          }
        if ( binfo.Name.length() == 0 )
          { // make up a name. FIXME: Possible buffer overflow w/ sprintf
          sprintf( obj_names[obj], "Unnamed block ID: %d Type: %s Size: %d",
            ids[obj], binfo.TypeName.length() ? binfo.TypeName.c_str() : "NULL", binfo.Size ); 
          binfo.Name = obj_names[obj];
          }
        this->DetermineVtkCellType( binfo );

        if ( binfo.AttributesPerEntry ) {
          char** attr_names;
          attr_names = (char**) malloc( binfo.AttributesPerEntry * sizeof(char*) );
          for ( j = 0; j < binfo.AttributesPerEntry; ++j )
            attr_names[j] = (char*) malloc( (MAX_STR_LENGTH + 1) * sizeof(char) );

          VTK_EXO_FUNC( ex_get_attr_names( exoid, obj_types[i], ids[obj], attr_names ), "Could not read attributes names." );

          for ( j = 0; j < binfo.AttributesPerEntry; ++j )
            {
            binfo.AttributeNames.push_back( attr_names[j] );
            binfo.AttributeStatus.push_back( 0 ); // don't load attributes by default
            }

          for ( j = 0; j < binfo.AttributesPerEntry; ++j )
            free( attr_names[j] );
          free( attr_names );
        }
        sortedObjects[binfo.Id] = (int) this->BlockInfo[obj_types[i]].size();
        this->BlockInfo[obj_types[i]].push_back( binfo );

      } else if ( OBJTYPE_IS_SET(i) ) {

        sinfo.Name = obj_names[obj];
        sinfo.Status = 0;
        sinfo.Id = ids[obj];

        VTK_EXO_FUNC( ex_get_set_param( exoid, obj_types[i], ids[obj], &sinfo.Size, &sinfo.DistFact ),
          "Could not read set parameters." );
        //num_entries = sinfo.Size;
        sinfo.FileOffset = setEntryFileOffset;
        setEntryFileOffset += sinfo.Size;
        if ( sinfo.Status )
          {
          sinfo.GridOffset = setEntryGridOffset;
          setEntryGridOffset += sinfo.Size;
          }
        else
          {
          sinfo.GridOffset = -1;
          }
        if ( sinfo.Name.length() == 0 )
          { // make up a name. FIXME: Possible buffer overflow w/ sprintf
          sprintf( obj_names[obj], "Unnamed set ID: %d Size: %d", ids[obj], sinfo.Size ); 
          sinfo.Name = obj_names[obj];
          }
        sortedObjects[sinfo.Id] = (int) this->SetInfo[obj_types[i]].size();
        this->SetInfo[obj_types[i]].push_back( sinfo );

      } else { /* object is map */

        minfo.Id = ids[obj];
        minfo.Status = obj == 0 ? 1 : 0; // only load the first map by default
        switch (obj_types[i]) {
        case vtkExodusIIReader::NODE_MAP:
          //num_entries = this->ModelParameters.num_nodes;
          minfo.Size = this->ModelParameters.num_nodes;
          break;
        case vtkExodusIIReader::EDGE_MAP:
          //num_entries = this->ModelParameters.num_edge;
          minfo.Size = this->ModelParameters.num_edge;
          break;
        case vtkExodusIIReader::FACE_MAP:
          //num_entries = this->ModelParameters.num_face;
          minfo.Size = this->ModelParameters.num_face;
          break;
        case vtkExodusIIReader::ELEM_MAP:
          //num_entries = this->ModelParameters.num_elem;
          minfo.Size = this->ModelParameters.num_elem;
          break;
        default:
          minfo.Size = 0;
        }
        minfo.Name = obj_names[obj];
        if ( minfo.Name.length() == 0 )
          { // make up a name. FIXME: Possible buffer overflow w/ sprintf
          sprintf( obj_names[obj], "Unnamed map ID: %d", ids[obj] ); 
          minfo.Name = obj_names[obj];
          }
        sortedObjects[minfo.Id] = (int) this->MapInfo[obj_types[i]].size();
        this->MapInfo[obj_types[i]].push_back( minfo );

      }

    } // end of loop over all object ids

    // Now that we have all objects of that type in the sortedObjects, we can
    // iterate over it to fill in the SortedObjectIndices (the map is a *sorted*
    // associative container)
    vtkstd::map<int,int>::iterator soit;
    for ( soit = sortedObjects.begin(); soit != sortedObjects.end(); ++soit )
      {
      this->SortedObjectIndices[obj_types[i]].push_back( soit->second );
      }

    if ( ((OBJTYPE_IS_BLOCK(i)) || (OBJTYPE_IS_SET(i))) && num_vars && num_timesteps > 0 )
      {
      this->ArrayInfo[obj_types[i]] = aitBlank;
      // Fill in ArrayInfo entries, combining array names into vectors/tensors where appropriate:
      this->GlomArrayNames( obj_types[i], nids, num_vars, var_names, truth_tab );
      }

    if ( var_names )
      {
      for ( j = 0; j < num_vars; ++j )
        free( var_names[j] );
      free( var_names );
      }
    if ( truth_tab )
      free( truth_tab );

    if ( nids )
      {
      free( ids );

      for ( obj = 0; obj < nids; ++obj )
        free( obj_names[obj] );
      free( obj_names );

      if ( OBJTYPE_IS_BLOCK(i) )
        {
        for ( obj = 0; obj < nids; ++obj )
          free( obj_typenames[obj] );
        free( obj_typenames );
        }
      }

  } // end of loop over all object types
  this->ComputeGridOffsets();

  // Now read information for nodal arrays
  VTK_EXO_FUNC( ex_get_var_param( exoid, "n", &num_vars ), "Unable to read number of nodal variables." );
  if ( num_vars > 0 )
    {
    ArrayInfoType ainfo;
    var_names = (char**) malloc( num_vars * sizeof(char*) );
    for ( j = 0; j < num_vars; ++j )
      var_names[j] = (char*) malloc( (MAX_STR_LENGTH + 1) * sizeof(char) );

    VTK_EXO_FUNC( ex_get_var_names( exoid, "n", num_vars, var_names ), "Could not read nodal variable names." );
    this->RemoveBeginningAndTrailingSpaces( num_vars, var_names );

    nids = 1;
    vtkstd::vector<int> dummy_truth;
    dummy_truth.reserve( num_vars );
    for ( j = 0; j < num_vars; ++j )
      {
      dummy_truth.push_back( 1 );
      }

    this->GlomArrayNames( vtkExodusIIReader::NODAL, nids, num_vars, var_names, &dummy_truth[0] );

    for ( j = 0; j < num_vars; ++j )
      {
      free( var_names[j] );
      }
    free( var_names );
    var_names = 0;
    }

  return 0;
}

int vtkExodusIIReaderPrivate::RequestData( vtkIdType timeStep, vtkUnstructuredGrid* output )
{
  // The work done here depends on several conditions:
  // - Has connectivity changed (i.e., has block/set status changed)?
  //   - If so, AND if point "squeeze" turned on, must reload points and re-squeeze.
  //   - If so, must re-assemble all arrays
  //   - Must recreate block/set id array.
  // - Has requested time changed?
  //   - If so, AND if "deflect mesh" turned on, must load new deflections and compute new points.
  //   - If so, must assemble all time-varying arrays for new time.
  // - Has array status changed?
  //   - If so, must delete old and/or load new arrays.
  // Obviously, many of these tasks overlap. For instance, it would be
  // foolish to re-assemble all the arrays when the connectivity has
  // changed and then toss them out in order to load arrays for a
  // different time step.

  // Caching strategy: use GLOBAL "object type" for assembled arrays.
  // If connectivity hasn't changed, then these arrays can be used;
  // otherwise, "raw" arrays must be used.
  // Pro:
  //   - single cache == easier bookkeeping (two caches would require us to decide how to equitably split avail mem between them)
  //   - many different operations are accelerated:
  //     - just changing which variables are loaded
  //     - changing which blocks are in output (doesn't require disk access if cache hit)
  //     - possible extension to single-node/cell over time
  // Con:
  //   - higher memory consumption for caching the same set of arrays (or, holding cache size fixed: fewer arrays fit)

  if ( ! output )
    {
    vtkErrorMacro( "You must specify an output mesh" );
    }

  // Connectivity first. Either from the cache or reassembled.
  // Connectivity isn't allowed to change with time step so this should only re-read
  // from disk when block/set status changes. And it might not even re-read then if
  // the cache contains all the requested block/set entries.
  this->AssembleOutputConnectivity( timeStep, output );

  // Now prepare points.
  // These shouldn't change unless the connectivity has changed.
  // This function doesn't apply displacements because we don't have the displacement vectors yet.
  this->AssembleOutputPoints( timeStep, output );

  // Finally, add the desired arrays from cache (or disk)
  // Point and cell arrays are handled differently because they have different problems
  // to solve. Point arrays must use the PointMap index to subset values while cell arrays
  // must be padded with zeros for cells in blocks/sets that do not contain the given arrays.
  this->AssembleOutputPointArrays( timeStep, output );
  this->AssembleOutputCellArrays( timeStep, output );

  this->AssembleOutputProceduralArrays( timeStep, output );

  this->AssembleOutputPointMaps( timeStep, output );
  this->AssembleOutputCellMaps( timeStep, output );

  this->CloseFile();

  return 0;
}

void vtkExodusIIReaderPrivate::Reset()
{
  this->CloseFile();
  this->BlockInfo.clear();
  this->SetInfo.clear();
  this->MapInfo.clear();
  this->SortedObjectIndices.clear();
  this->ArrayInfo.clear();
  this->ExodusVersion = -1.;
  this->Times.clear();
  this->TimeStep = 0;
  this->NumberOfCells = 0;
  this->SqueezePoints = 1;
  this->PointMap.clear();
  this->Cache->Clear();
  this->ApplyDisplacements = 1;
  this->DisplacementMagnitude = 1.;
  memset( (void*)&this->ModelParameters, 0, sizeof(this->ModelParameters) );
  this->Cache->SetCacheCapacity( 0. ); // FIXME: Perhaps Cache should have a Reset and a Clear method?
  this->Cache->SetCacheCapacity( 128. ); // FIXME: Perhaps Cache should have a Reset and a Clear method?
  this->SetCachedConnectivity( 0 );
  this->NextSqueezePoint = 0;
  this->GenerateGlobalElementIdArray = 0;
  this->GenerateGlobalNodeIdArray = 0;
  this->GenerateObjectIdArray = 1;

  this->Modified();
}

void vtkExodusIIReaderPrivate::SetSqueezePoints( int sp )
{
  if ( this->SqueezePoints == sp )
    return;

  this->SqueezePoints = sp;
  this->Modified();

  // Invalidate global "topology" cache
  this->SetCachedConnectivity( 0 );

  // The point map should be invalidated
  this->PointMap.clear();
  this->NextSqueezePoint = 0;
}

int vtkExodusIIReaderPrivate::GetNumberOfNodes()
{
  if ( this->SqueezePoints )
    return this->NextSqueezePoint;
  return this->ModelParameters.num_nodes;
}

int vtkExodusIIReaderPrivate::GetNumberOfObjectsOfType( int otyp )
{
  int i = this->GetObjectTypeIndexFromObjectType( otyp );
  if ( i < 0 )
    {
    // Could signal warning here, but might not want it if file simply doesn't have objects of some obscure type (e.g., edge sets)
    return 0;
    }
  return this->GetNumberOfObjectsAtTypeIndex( i );
}

int vtkExodusIIReaderPrivate::GetNumberOfObjectArraysOfType( int otyp )
{
  vtkstd::map<int,vtkstd::vector<ArrayInfoType> >::iterator it = this->ArrayInfo.find( otyp );
  if ( it != this->ArrayInfo.end() )
    {
    return (int) it->second.size();
    }
  // Could signal warning here, but might not want it if file simply doesn't have objects of some obscure type (e.g., edge sets)
  return 0;
}

const char* vtkExodusIIReaderPrivate::GetObjectName( int otyp, int k )
{
  ObjectInfoType* oinfop = this->GetSortedObjectInfo( otyp, k );
  return oinfop ? oinfop->Name.c_str() : 0;
}

int vtkExodusIIReaderPrivate::GetObjectId( int otyp, int k )
{
  ObjectInfoType* oinfop = this->GetSortedObjectInfo( otyp, k );
  return oinfop ? oinfop->Id : -1;
}

int vtkExodusIIReaderPrivate::GetObjectSize( int otyp, int k )
{
  ObjectInfoType* oinfop = this->GetSortedObjectInfo( otyp, k );
  return oinfop ? oinfop->Size : 0;
}

int vtkExodusIIReaderPrivate::GetObjectStatus( int otyp, int k )
{
  ObjectInfoType* oinfop = this->GetSortedObjectInfo( otyp, k );
  return oinfop ? oinfop->Status : 0;
}

void vtkExodusIIReaderPrivate::SetObjectStatus( int otyp, int k, int stat )
{
  stat = (stat != 0); // Force stat to be either 0 or 1
  // OK, found the object
  ObjectInfoType* oinfop = this->GetSortedObjectInfo( otyp, k );
  if ( ! oinfop )
    { // error message will have been generated by GetSortedObjectInfo()
    return;
    }

  if ( oinfop->Status == stat )
    { // no change => do nothing
    return;
    }
  oinfop->Status = stat;
  this->ComputeGridOffsets();

  // Invalidate connectivity
  this->SetCachedConnectivity( 0 );

  // Invalidate global cell arrays
  vtkExodusIICacheKey pattern( 0, 1, 0, 0 );
  this->Cache->Invalidate( vtkExodusIICacheKey( 0, vtkExodusIIReader::GLOBAL, 0, 0 ), pattern );
  pattern = vtkExodusIICacheKey( 1, 1, 0, 0 );
  this->Cache->Invalidate( vtkExodusIICacheKey( -1, vtkExodusIIReader::GLOBAL_OBJECT_ID, 0, 0 ), pattern );
  this->Cache->Invalidate( vtkExodusIICacheKey( -1, vtkExodusIIReader::GLOBAL_ELEMENT_ID, 0, 0 ), pattern );
  this->Cache->Invalidate( vtkExodusIICacheKey( -1, vtkExodusIIReader::GLOBAL_NODE_ID, 0, 0 ), pattern );

  this->Modified();
}

const char* vtkExodusIIReaderPrivate::GetObjectArrayName( int otyp, int i )
{
  vtkstd::map<int,vtkstd::vector<ArrayInfoType> >::iterator it = this->ArrayInfo.find( otyp );
  if ( it != this->ArrayInfo.end() )
    {
    int N = (int) it->second.size();
    if ( i < 0 || i >= N )
      {
      vtkWarningMacro( "You requested array " << i << " in a collection of only " << N << " arrays." );
      return 0;
      }
    return it->second[i].Name.c_str();
    }
  vtkWarningMacro( "Could not find collection of arrays for objects of type " << otyp <<
    " (" << objtype_names[this->GetObjectTypeIndexFromObjectType(otyp)] << ")." );
  return 0;
}

int vtkExodusIIReaderPrivate::GetNumberOfObjectArrayComponents( int otyp, int i )
{
  vtkstd::map<int,vtkstd::vector<ArrayInfoType> >::iterator it = this->ArrayInfo.find( otyp );
  if ( it != this->ArrayInfo.end() )
    {
    int N = (int) it->second.size();
    if ( i < 0 || i >= N )
      {
      vtkWarningMacro( "You requested array " << i << " in a collection of only " << N << " arrays." );
      return 0;
      }
    return it->second[i].Components;
    }
  vtkWarningMacro( "Could not find collection of arrays for objects of type " << otyp <<
    " (" << objtype_names[this->GetObjectTypeIndexFromObjectType(otyp)] << ")." );
  return 0;
}

int vtkExodusIIReaderPrivate::GetObjectArrayStatus( int otyp, int i )
{
  vtkstd::map<int,vtkstd::vector<ArrayInfoType> >::iterator it = this->ArrayInfo.find( otyp );
  if ( it != this->ArrayInfo.end() )
    {
    int N = (int) it->second.size();
    if ( i < 0 || i >= N )
      {
      vtkWarningMacro( "You requested array " << i << " in a collection of only " << N << " arrays." );
      return 0;
      }
    return it->second[i].Status;
    }
  vtkWarningMacro( "Could not find collection of arrays for objects of type " << otyp <<
    " (" << objtype_names[this->GetObjectTypeIndexFromObjectType(otyp)] << ")." );
  return 0;
}

void vtkExodusIIReaderPrivate::SetObjectArrayStatus( int otyp, int i, int stat )
{
  stat = ( stat != 0 ); // Force stat to be either 0 or 1
  vtkstd::map<int,vtkstd::vector<ArrayInfoType> >::iterator it = this->ArrayInfo.find( otyp );
  if ( it != this->ArrayInfo.end() )
    {
    int N = (int) it->second.size();
    if ( i < 0 || i >= N )
      {
      vtkWarningMacro( "You requested array " << i << " in a collection of only " << N << " arrays." );
      return;
      }
    if ( it->second[i].Status == stat )
      {
      // no change => do nothing
      return;
      }
    it->second[i].Status = stat;
    this->Modified();
    // FIXME: Mark something so we know what's changed since the last RequestData?!
    // For the "global" (assembled) array, this is tricky because we really only want
    // to invalidate a range of the total array... For now, we'll just force the "global"
    // array to be reassembled even if it does mean a lot more copying -- it's not like
    // it was any faster before.
    //vtkExodusIICacheKey key( 0, GLOBAL, 0, i );
    //vtkExodusIICacheKey pattern( 0, 1, 0, 1 );
    this->Cache->Invalidate(
      vtkExodusIICacheKey( 0, vtkExodusIIReader::GLOBAL, otyp, i ),
      vtkExodusIICacheKey( 0, 1, 1, 1 ) );
    }
  else
    {
    vtkWarningMacro( "Could not find collection of arrays for objects of type " << otyp <<
      " (" << objtype_names[this->GetObjectTypeIndexFromObjectType(otyp)] << ")." );
    }
}

int vtkExodusIIReaderPrivate::GetNumberOfObjectAttributes( int otyp, int oi )
{
  vtkstd::map<int,vtkstd::vector<BlockInfoType> >::iterator it = this->BlockInfo.find( otyp );
  if ( it != this->BlockInfo.end() )
    {
    int N = (int) it->second.size();
    if ( oi < 0 || oi >= N )
      {
      vtkWarningMacro( "You requested block " << oi << " in a collection of only " << N << " blocks." );
      return 0;
      }
    return (int) it->second[oi].AttributeNames.size();
    }
  vtkWarningMacro( "Could not find collection of blocks of type " << otyp <<
    " (" << objtype_names[this->GetObjectTypeIndexFromObjectType(otyp)] << ")." );
  return 0;
}

const char* vtkExodusIIReaderPrivate::GetObjectAttributeName( int otyp, int oi, int ai )
{
  vtkstd::map<int,vtkstd::vector<BlockInfoType> >::iterator it = this->BlockInfo.find( otyp );
  if ( it != this->BlockInfo.end() )
    {
    int N = (int) it->second.size();
    if ( oi < 0 || oi >= N )
      {
      vtkWarningMacro( "You requested block " << oi << " in a collection of only " << N << " blocks." );
      return 0;
      }
    oi = this->SortedObjectIndices[otyp][oi]; // index into sorted list of objects (block order, not file order)
    N = (int) it->second[oi].AttributeNames.size();
    if ( ai < 0 || ai >= N )
      {
      vtkWarningMacro( "You requested attribute " << ai << " in a collection of only " << N << " attributes." );
      return 0;
      }
    else
      {
      return it->second[oi].AttributeNames[ai].c_str();
      }
    }
  vtkWarningMacro( "Could not find collection of blocks of type " << otyp <<
    " (" << objtype_names[this->GetObjectTypeIndexFromObjectType(otyp)] << ")." );
  return 0;
}

int vtkExodusIIReaderPrivate::GetObjectAttributeIndex( int otyp, int oi, const char* attribName )
{
  vtkstd::map<int,vtkstd::vector<BlockInfoType> >::iterator it = this->BlockInfo.find( otyp );
  if ( it != this->BlockInfo.end() )
    {
    int N = (int) it->second.size();
    if ( oi < 0 || oi >= N )
      {
      vtkWarningMacro( "You requested block " << oi << " in a collection of only " << N << " blocks." );
      return -1;
      }
    oi = this->SortedObjectIndices[otyp][oi]; // index into sorted list of objects (block order, not file order)
    N = (int) it->second[oi].AttributeNames.size();
    int ai;
    for ( ai = 0; ai < N; ++ai )
      {
      if ( it->second[oi].AttributeNames[ai] == attribName )
        {
        return ai;
        }
      }
    return -1;
    }
  vtkWarningMacro( "Could not find collection of blocks of type " << otyp <<
    " (" << objtype_names[this->GetObjectTypeIndexFromObjectType(otyp)] << ")." );
  return -1;
}

int vtkExodusIIReaderPrivate::GetObjectAttributeStatus( int otyp, int oi, int ai )
{
  vtkstd::map<int,vtkstd::vector<BlockInfoType> >::iterator it = this->BlockInfo.find( otyp );
  if ( it != this->BlockInfo.end() )
    {
    int N = (int) it->second.size();
    if ( oi < 0 || oi >= N )
      {
      vtkWarningMacro( "You requested block " << oi << " in a collection of only " << N << " blocks." );
      return 0;
      }
    oi = this->SortedObjectIndices[otyp][oi]; // index into sorted list of objects (block order, not file order)
    N = (int) it->second[oi].AttributeStatus.size();
    if ( ai < 0 || ai >= N )
      {
      vtkWarningMacro( "You requested attribute " << ai << " in a collection of only " << N << " attributes." );
      return 0;
      }
    else
      {
      return it->second[oi].AttributeStatus[ai];
      }
    }
  vtkWarningMacro( "Could not find collection of blocks of type " << otyp <<
    " (" << objtype_names[this->GetObjectTypeIndexFromObjectType(otyp)] << ")." );
  return 0;
}

void vtkExodusIIReaderPrivate::SetObjectAttributeStatus( int otyp, int oi, int ai, int status )
{
  status = status ? 1 : 0;
  vtkstd::map<int,vtkstd::vector<BlockInfoType> >::iterator it = this->BlockInfo.find( otyp );
  if ( it != this->BlockInfo.end() )
    {
    int N = (int) it->second.size();
    if ( oi < 0 || oi >= N )
      {
      vtkWarningMacro( "You requested block " << oi << " in a collection of only " << N << " blocks." );
      return;
      }
    oi = this->SortedObjectIndices[otyp][oi]; // index into sorted list of objects (block order, not file order)
    N = (int) it->second[oi].AttributeStatus.size();
    if ( ai < 0 || ai >= N )
      {
      vtkWarningMacro( "You requested attribute " << ai << " in a collection of only " << N << " attribute." );
      return;
      }
    else
      {
      if ( it->second[oi].AttributeStatus[ai] == status )
        {
        return;
        }
      it->second[oi].AttributeStatus[ai] = status;
      this->Modified();
      }
    }
  vtkWarningMacro( "Could not find collection of blocks of type " << otyp <<
    " (" << objtype_names[this->GetObjectTypeIndexFromObjectType(otyp)] << ")." );
}

void vtkExodusIIReaderPrivate::SetApplyDisplacements( int d )
{
  if ( this->ApplyDisplacements == d )
    return;

  this->ApplyDisplacements = d;
  this->Modified();

  // Require the coordinates to be recomputed:
  this->Cache->Invalidate(
    vtkExodusIICacheKey( 0, vtkExodusIIReader::NODAL_COORDS, 0, 0 ),
    vtkExodusIICacheKey( 0, 1, 0, 0 ) );
}

void vtkExodusIIReaderPrivate::SetDisplacementMagnitude( double s )
{
  if ( this->DisplacementMagnitude == s )
    return;

  this->DisplacementMagnitude = s;
  this->Modified();

  // Require the coordinates to be recomputed:
  this->Cache->Invalidate(
    vtkExodusIICacheKey( 0, vtkExodusIIReader::NODAL_COORDS, 0, 0 ),
    vtkExodusIICacheKey( 0, 1, 0, 0 ) );
}

vtkDataArray* vtkExodusIIReaderPrivate::FindDisplacementVectors( int timeStep )
{
  vtkstd::map<int,vtkstd::vector<ArrayInfoType> >::iterator it = this->ArrayInfo.find( vtkExodusIIReader::NODAL );
  if ( it != this->ArrayInfo.end() )
    {
    int N = (int) it->second.size();
    for ( int i = 0; i < N; ++i )
      {
      vtkstd::string upperName = vtksys::SystemTools::UpperCase( it->second[i].Name.substr( 0, 4 ) );
      if ( upperName == "DISP" && it->second[i].Components == 3 )
        {
        return this->GetCacheOrRead( vtkExodusIICacheKey( timeStep, vtkExodusIIReader::NODAL, 0, i ) );
        }
      }
    }
  return 0;
}


// ------------------------------------------------------------ XML PARSER CLASS

class vtkExodusIIXMLParser : public vtkXMLParser
{
protected:
  vtkExodusIIReaderPrivate* Metadata;
  int InMaterialAssignment;

public:
  static vtkExodusIIXMLParser* New();
  vtkTypeRevisionMacro(vtkExodusIIXMLParser,vtkXMLParser);
  void Go(  const char* xmlFileName, vtkExodusIIReaderPrivate* metadata )
    {
    this->InMaterialAssignment = 0;
    if ( ! xmlFileName || ! metadata )
      {
      vtkErrorMacro( "Must have a valid filename and metadata object to open XML file." );
      }
    else
      {
      this->Metadata = metadata;
      //this->Metadata->Register( this );
      this->SetFileName( xmlFileName );
      this->Parse();
      this->Metadata = 0;
      }
    }
protected:
  vtkExodusIIXMLParser()
    {
    this->Metadata = 0;
    this->InMaterialAssignment = 0;
    }
  virtual ~vtkExodusIIXMLParser()
    {
    //this->Metadata->UnRegister( this );
    }
  virtual void StartElement( const char* tagName, const char** attrs )
    {
    (void)attrs; //FIXME: Useme
    const char* name = strrchr( tagName, ':' );
    name = name ? name + 1 : tagName; // If tag name has xml namespace separator, get rid of namespace.
    vtkStdString tName( name );

    if ( tName == "assembly" )
      {
      //this->Metadata->AddAssembly( tName, this->ParentAssembly );
      cout << name << "\n";
      }
    else if ( tName == "part" )
      {
      //this->Metadata->AddPart( pnum, inst, curAssy );
      cout << name << "\n";
      }
    else if ( tName == "material-specification" )
      {
      //matl = this->Metadata->AddMatl( matname );
      //this->Metadata->SetPartMaterial( this->CurrentPart, inst, matl );
      cout << name << "\n";
      }
    else if ( tName == "blocks" )
      {
      /*
      this->Metadata->AddPartBlock( pnum, blocktype, block id );
      if ( this->InMaterialAssignment )
        {
        this->Metadata->SetPartMaterial( this->CurrentPart, inst, matl );
        }
       */
      cout << name << "\n";
      }
    else if ( tName == "block" )
      {
      //this->Metadata->SetBlockName( this->GetBlockType( attrs ), blockid );
      cout << name << "\n";
      }
    else if ( tName == "material-assignments" )
      {
      this->InMaterialAssignment = 1;
      cout << name << "\n";
      }
    else if ( tName == "material" )
      {
      cout << name << "\n";
      }
    }
  virtual void EndElement( const char* tagName )
    {
    const char* name = strrchr( tagName, ':' );
    name = name ? name + 1 : tagName; // If tag name has xml namespace separator, get rid of namespace.
    vtkStdString tName( name );

    if ( tName == "assembly" )
      {
      // update curAssy
      }
    else if ( tName == "part" )
      {
      }
    else if ( tName == "material-specification" )
      {
      }
    else if ( tName == "blocks" )
      {
      }
    else if ( tName == "block" )
      {
      }
    else if ( tName == "material-assignments" )
      {
      this->InMaterialAssignment = 0;
      }
    else if ( tName == "material" )
      {
      }
    }
};

vtkStandardNewMacro(vtkExodusIIXMLParser);
vtkCxxRevisionMacro(vtkExodusIIXMLParser,"$Revision$");

// -------------------------------------------------------- PUBLIC CLASS MEMBERS

vtkCxxRevisionMacro(vtkExodusIIReader,"$Revision$");
vtkStandardNewMacro(vtkExodusIIReader);
vtkCxxSetObjectMacro(vtkExodusIIReader,Metadata,vtkExodusIIReaderPrivate);
vtkCxxSetObjectMacro(vtkExodusIIReader,ExodusModel,vtkExodusModel);
vtkCxxSetObjectMacro(vtkExodusIIReader,Parser,vtkExodusIIXMLParser);

vtkExodusIIReader::vtkExodusIIReader()
{
  this->FileName = 0;
  this->XMLFileName = 0;
  this->Metadata = vtkExodusIIReaderPrivate::New();
  this->Metadata->Parent = this;
  this->TimeStep = 0;
  this->TimeStepRange[0] = 0;
  this->TimeStepRange[1] = 0;
  this->Parser = 0;
  this->ExodusModelMetadata = 0;
  this->PackExodusModelOntoOutput = 1;
  this->ExodusModel = 0;
  this->DisplayType = 0;

  //begin USE_EXO_DSP_FILTERS
  this->DSPFilteringIsEnabled = 0;
  this->DSPFilters = 0;
  this->AddingFilter = vtkDSPFilterDefinition::New();
  //end USE_EXO_DSP_FILTERS


  this->SetNumberOfInputPorts( 0 );
}

vtkExodusIIReader::~vtkExodusIIReader()
{
  //begin USE_EXO_DSP_FILTERS
  int neb = this->GetNumberOfElementBlockArrays();
  if ( this->DSPFilters )
    {
    for ( int i = 0; i < neb; ++i )
      {
      this->DSPFilters[i]->Delete();
      }
    delete [] this->DSPFilters ;
    this->DSPFilters = 0;
    }
  if ( this->AddingFilter )
    {
    this->AddingFilter->Delete();
    }
  //end USE_EXO_DSP_FILTERS

  // The following must come *after* DSPFilters are deleted since neb
  // requires data that's reset inside SetFileName().
  this->SetXMLFileName( 0 );
  this->SetFileName( 0 );

  this->SetMetadata( 0 );
  this->SetExodusModel( 0 );
  this->SetParser( 0 );
}

// Normally, vtkExodusIIReader::PrintSelf would be here.
// But it's above to prevent PrintSelf-Hybrid from failing because it assumes
// the first PrintSelf method is the one for the class declared in the header file.

int vtkExodusIIReader::CanReadFile( const char* fname )
{
  int exoid;
  int appWordSize = 8;
  int diskWordSize = 8;
  float version;
  if ( (exoid = ex_open( fname, EX_READ, &appWordSize, &diskWordSize, &version )) == 0 )
    {
    return 0;
    }
  if ( ex_close( exoid ) != 0 )
    {
    vtkWarningMacro( "Unable to close \"" << fname << "\" opened for testing." );
    return 0;
    }
  return 1;
}

unsigned long vtkExodusIIReader::GetMTime()
{
  unsigned long mtime1, mtime2;
  unsigned long readerMTime = this->MTime.GetMTime();
  unsigned long privateMTime = this->Metadata->GetMTime();
  unsigned long fileNameMTime = this->FileNameMTime.GetMTime();
  unsigned long xmlFileNameMTime = this->XMLFileNameMTime.GetMTime();
  mtime1 = privateMTime > readerMTime ? privateMTime : readerMTime;
  mtime2 = fileNameMTime > xmlFileNameMTime ? fileNameMTime : xmlFileNameMTime;
  return mtime1 > mtime2 ? mtime1 : mtime2;
}

unsigned long vtkExodusIIReader::GetMetadataMTime()
{
  return this->Metadata->GetMTime();
}

#define vtkSetStringMacroBody(propName,fname) \
  int modified = 0; \
  if ( fname == this->propName ) \
    return; \
  if ( fname && this->propName && !strcmp( fname, this->propName ) ) \
    return; \
  modified = 1; \
  if ( this->propName ) \
    delete [] this->propName; \
  if ( fname ) \
    { \
    size_t fnl = strlen( fname ) + 1; \
    char* dst = new char[fnl]; \
    const char* src = fname; \
    this->propName = dst; \
    do { *dst++ = *src++; } while ( --fnl ); \
    } \
  else \
    { \
    this->propName = 0; \
    }

void vtkExodusIIReader::SetFileName( const char* fname )
{
  vtkSetStringMacroBody(FileName,fname);
  if ( modified )
    {
    this->Metadata->Reset();
    this->FileNameMTime.Modified();
    }
}

void vtkExodusIIReader::SetXMLFileName( const char* fname )
{
  vtkSetStringMacroBody(XMLFileName,fname);
  if ( modified )
    {
    this->XMLFileNameMTime.Modified();
    }
}

int vtkExodusIIReader::RequestInformation(
  vtkInformation* vtkNotUsed(request),
  vtkInformationVector** vtkNotUsed(inputVector),
  vtkInformationVector* outputVector )
{
  int newMetadata = 0;
  vtkInformation* outInfo = outputVector->GetInformationObject(0);

  // If the metadata is older than the filename
  if ( this->Metadata->GetMTime() < this->FileNameMTime )
    {
    if ( this->Metadata->OpenFile( this->FileName ) )
      {
      this->Metadata->RequestInformation();
      this->Metadata->CloseFile();
      newMetadata = 1;
      }
    else
      {
      vtkErrorMacro( "Unable to open file \"" << (this->FileName ? this->FileName : "(null)") << "\" to read metadata" );
      return 0;
      }
    }

  // If the XML filename exists and is newer than any existing parser (or there is no parser), reread XML file.
  if (
    ( this->Parser && this->Parser->GetMTime() < this->XMLFileNameMTime && this->XMLFileName ) ||
    ( ! this->Parser ) )
    {
    if ( this->Parser )
      {
      this->Parser->Delete();
      this->Parser = 0;
      }

    int gotXMLFile = 0;
    if ( ! this->XMLFileName || ! vtksys::SystemTools::FileExists( this->XMLFileName ) )
      {
      if ( this->FileName )
        {
        vtkStdString baseName( vtksys::SystemTools::GetFilenameWithoutExtension( this->FileName ) );
        vtkStdString xmlExt( baseName + ".xml" );
        if ( vtksys::SystemTools::FileExists( xmlExt ) )
          {
          this->SetXMLFileName( xmlExt.c_str() );
          gotXMLFile = 1;
          }
        if ( ! gotXMLFile )
          {
          vtkStdString dartExt( baseName + ".dart" );
          if ( vtksys::SystemTools::FileExists( dartExt ) )
            {
            this->SetXMLFileName( dartExt.c_str() );
            gotXMLFile = 1;
            }
          }
        if ( ! gotXMLFile )
          {
          vtkStdString baseDir( vtksys::SystemTools::GetFilenamePath( this->FileName ) );
          vtkStdString artifact( baseDir + "/artifact.dta" );
          if ( vtksys::SystemTools::FileExists( artifact ) )
            {
            this->SetXMLFileName( artifact.c_str() );
            gotXMLFile = 1;
            }
          }
        if ( ! gotXMLFile )
          { // Catch the case where filename was non-NULL but didn't exist.
          this->SetXMLFileName( 0 );
          }
        }
      }
    else
      {
      gotXMLFile = 1;
      }

    if ( gotXMLFile )
      {
      this->Parser = vtkExodusIIXMLParser::New();
      // Now overwrite any names in the exodus file with names from XML file.
      this->Parser->Go( this->XMLFileName, this->Metadata );
      }
    }

  int nTimes = (int) this->Metadata->Times.size();
  double timeRange[2];
  if ( nTimes )
    {
    timeRange[0] = this->Metadata->Times[0];
    timeRange[1] = this->Metadata->Times[nTimes - 1];
    outInfo->Set( vtkStreamingDemandDrivenPipeline::TIME_STEPS(), &this->Metadata->Times[0], nTimes );
    outInfo->Set( vtkStreamingDemandDrivenPipeline::TIME_RANGE(), timeRange, 2 );
    this->TimeStepRange[0] = 0; this->TimeStepRange[1] = nTimes - 1;
    }

  if ( newMetadata )
    {
    // update ExodusModelMetadata
    }

  return 1;
}

int vtkExodusIIReader::RequestData(
  vtkInformation* vtkNotUsed(request),
  vtkInformationVector** vtkNotUsed(inputVector),
  vtkInformationVector* outputVector )
{
  if ( ! this->FileName || ! this->Metadata->OpenFile( this->FileName ) )
    {
    vtkErrorMacro( "Unable to open file \"" << (this->FileName ? this->FileName : "(null)") << "\" to read data" );
    return 0;
    }

  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  vtkUnstructuredGrid *output = vtkUnstructuredGrid::SafeDownCast( outInfo->Get( vtkDataObject::DATA_OBJECT() ) );

  // Check if a particular time was requested.
  int timeStep = this->TimeStep;
  if(outInfo->Has(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEPS()))
    { // Get the requested time step. We only supprt requests of a single time step in this reader right now
    double* requestedTimeSteps = outInfo->Get( vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEPS() );

    // Save the time value in the output data information.
    int length = outInfo->Length( vtkStreamingDemandDrivenPipeline::TIME_STEPS() );
    double* steps = outInfo->Get( vtkStreamingDemandDrivenPipeline::TIME_STEPS() );

    // find the closest time step
    timeStep = 0;
    while (timeStep < length - 1 && steps[timeStep] < requestedTimeSteps[0])
      {
      timeStep++;
      }
    this->TimeStep = timeStep;
    output->GetInformation()->Set( vtkDataObject::DATA_TIME_STEPS(), steps + timeStep, 1 );
    }

  //cout << "Requesting step " << timeStep << " for output " << output << "\n";
  this->Metadata->RequestData( timeStep, output );

  //begin USE_EXO_DSP_FILTERS
  this->GetDSPOutputArrays( this->Metadata->Exoid, timeStep, output );
  //end USE_EXO_DSP_FILTERS

  return 1;
}

void vtkExodusIIReader::SetGenerateObjectIdCellArray( int x ) { this->Metadata->SetGenerateObjectIdArray( x ); }
int vtkExodusIIReader::GetGenerateObjectIdCellArray() { return this->Metadata->GetGenerateObjectIdArray(); }

void vtkExodusIIReader::SetGenerateGlobalElementIdArray( int x ) { this->Metadata->SetGenerateGlobalElementIdArray( x ); }
int vtkExodusIIReader::GetGenerateGlobalElementIdArray() { return this->Metadata->GetGenerateGlobalElementIdArray(); }

void vtkExodusIIReader::SetGenerateGlobalNodeIdArray( int x ) { this->Metadata->SetGenerateGlobalNodeIdArray( x ); }
int vtkExodusIIReader::GetGenerateGlobalNodeIdArray() { return this->Metadata->GetGenerateGlobalNodeIdArray(); }

// FIXME: Implement the four functions that return ID_NOT_FOUND below.
int vtkExodusIIReader::GetGlobalElementID( vtkDataSet* data, int localID )
{ return GetGlobalElementID( data, localID, SEARCH_TYPE_ELEMENT_THEN_NODE ); }
int vtkExodusIIReader::GetGlobalElementID ( vtkDataSet* data, int localID, int searchType )
{ (void)data; (void)localID; (void)searchType; return ID_NOT_FOUND; }

int vtkExodusIIReader::GetGlobalFaceID( vtkDataSet* data, int localID )
{ return GetGlobalFaceID( data, localID, SEARCH_TYPE_ELEMENT_THEN_NODE ); }
int vtkExodusIIReader::GetGlobalFaceID ( vtkDataSet* data, int localID, int searchType )
{ (void)data; (void)localID; (void)searchType; return ID_NOT_FOUND; }

int vtkExodusIIReader::GetGlobalEdgeID( vtkDataSet* data, int localID )
{ return GetGlobalEdgeID( data, localID, SEARCH_TYPE_ELEMENT_THEN_NODE ); }
int vtkExodusIIReader::GetGlobalEdgeID ( vtkDataSet* data, int localID, int searchType )
{ (void)data; (void)localID; (void)searchType; return ID_NOT_FOUND; }

int vtkExodusIIReader::GetGlobalNodeID( vtkDataSet* data, int localID )
{ return GetGlobalNodeID( data, localID, SEARCH_TYPE_NODE_THEN_ELEMENT ); }
int vtkExodusIIReader::GetGlobalNodeID( vtkDataSet* data, int localID, int searchType )
{ (void)data; (void)localID; (void)searchType; return ID_NOT_FOUND; }

void vtkExodusIIReader::SetApplyDisplacements( int d )
{
  this->Metadata->SetApplyDisplacements( d );
}
int vtkExodusIIReader::GetApplyDisplacements()
{
  return this->Metadata->GetApplyDisplacements();
}

void vtkExodusIIReader::SetDisplacementMagnitude( float s )
{
  this->Metadata->SetDisplacementMagnitude( s );
}
float vtkExodusIIReader::GetDisplacementMagnitude()
{
  return this->Metadata->GetDisplacementMagnitude();
}

const char* vtkExodusIIReader::GetTitle() { return this->Metadata->ModelParameters.title; }
int vtkExodusIIReader::GetDimensionality() { return this->Metadata->ModelParameters.num_dim; }
int vtkExodusIIReader::GetNumberOfTimeSteps() { return (int) this->Metadata->Times.size(); }

int vtkExodusIIReader::GetNumberOfNodesInFile() { return this->Metadata->ModelParameters.num_nodes; }
int vtkExodusIIReader::GetNumberOfEdgesInFile() { return this->Metadata->ModelParameters.num_edge; }
int vtkExodusIIReader::GetNumberOfFacesInFile() { return this->Metadata->ModelParameters.num_face; }
int vtkExodusIIReader::GetNumberOfElementsInFile() { return this->Metadata->ModelParameters.num_elem; }

int vtkExodusIIReader::GetNumberOfObjects( int objectType )
{
  return this->Metadata->GetNumberOfObjectsOfType( objectType );
}

int vtkExodusIIReader::GetObjectTypeFromName( const char* name )
{
  vtkStdString tname( name );
  if ( tname == "edge" ) return EDGE_BLOCK; 
  else if ( tname == "face" ) return FACE_BLOCK; 
  else if ( tname == "element" ) return ELEM_BLOCK; 
  else if ( tname == "node set" ) return NODE_SET; 
  else if ( tname == "edge set" ) return EDGE_SET; 
  else if ( tname == "face set" ) return FACE_SET; 
  else if ( tname == "side set" ) return SIDE_SET; 
  else if ( tname == "element set" ) return ELEM_SET; 
  else if ( tname == "node map" ) return NODE_MAP; 
  else if ( tname == "edge map" ) return EDGE_MAP; 
  else if ( tname == "face map" ) return FACE_MAP; 
  else if ( tname == "element map" ) return ELEM_MAP; 
  else if ( tname == "grid" ) return GLOBAL; 
  else if ( tname == "node" ) return NODAL; 
  else if ( tname == "assembly" ) return ASSEMBLY; 
  else if ( tname == "part" ) return PART; 
  else if ( tname == "material" ) return MATERIAL; 
  else if ( tname == "hierarchy" ) return HIERARCHY; 
  else if ( tname == "cell" ) return GLOBAL_CONN; 
  else if ( tname == "element block cell" ) return ELEM_BLOCK_ELEM_CONN; 
  else if ( tname == "element block face" ) return ELEM_BLOCK_FACE_CONN; 
  else if ( tname == "element block edge" ) return ELEM_BLOCK_EDGE_CONN; 
  else if ( tname == "face block cell" ) return FACE_BLOCK_CONN; 
  else if ( tname == "edge block cell" ) return EDGE_BLOCK_CONN; 
  else if ( tname == "element set cell" ) return ELEM_SET_CONN; 
  else if ( tname == "side set cell" ) return SIDE_SET_CONN; 
  else if ( tname == "face set cell" ) return FACE_SET_CONN; 
  else if ( tname == "edge set cell" ) return EDGE_SET_CONN; 
  else if ( tname == "node set cell" ) return NODE_SET_CONN; 
  else if ( tname == "nodal coordinates" ) return NODAL_COORDS; 
  else if ( tname == "object id" ) return GLOBAL_OBJECT_ID; 
  else if ( tname == "global element id" ) return GLOBAL_ELEMENT_ID; 
  else if ( tname == "global node id" ) return GLOBAL_NODE_ID; 
  else if ( tname == "element id" ) return ELEMENT_ID; 
  else if ( tname == "node id" ) return NODE_ID; 
  else if ( tname == "pointmap" ) return NODAL_SQUEEZEMAP; 
  return -1;
}

const char* vtkExodusIIReader::GetObjectTypeName( int otyp )
{
  switch ( otyp )
    {
  case EDGE_BLOCK: return "edge";
  case FACE_BLOCK: return "face";
  case ELEM_BLOCK: return "element";
  case NODE_SET: return "node set";
  case EDGE_SET: return "edge set";
  case FACE_SET: return "face set";
  case SIDE_SET: return "side set";
  case ELEM_SET: return "element set";
  case NODE_MAP: return "node map";
  case EDGE_MAP: return "edge map";
  case FACE_MAP: return "face map";
  case ELEM_MAP: return "element map";
  case GLOBAL: return "grid";
  case NODAL: return "node";
  case ASSEMBLY: return "assembly";
  case PART: return "part";
  case MATERIAL: return "material";
  case HIERARCHY: return "hierarchy";
  case GLOBAL_CONN: return "cell";
  case ELEM_BLOCK_ELEM_CONN: return "element block cell";
  case ELEM_BLOCK_FACE_CONN: return "element block face";
  case ELEM_BLOCK_EDGE_CONN: return "element block edge";
  case FACE_BLOCK_CONN: return "face block cell";
  case EDGE_BLOCK_CONN: return "edge block cell";
  case ELEM_SET_CONN: return "element set cell";
  case SIDE_SET_CONN: return "side set cell";
  case FACE_SET_CONN: return "face set cell";
  case EDGE_SET_CONN: return "edge set cell";
  case NODE_SET_CONN: return "node set cell";
  case NODAL_COORDS: return "nodal coordinates";
  case GLOBAL_OBJECT_ID: return "object id";
  case GLOBAL_ELEMENT_ID: return "global element id";
  case GLOBAL_NODE_ID: return "global node id";
  case ELEMENT_ID: return "element id";
  case NODE_ID: return "node id";
  case NODAL_SQUEEZEMAP: return "pointmap";
    }
  return 0;
}

int vtkExodusIIReader::GetNumberOfNodes() { return this->Metadata->GetNumberOfNodes(); }

int vtkExodusIIReader::GetNumberOfEntriesInObject( int objectType, int objectIndex )
{
  return this->Metadata->GetObjectSize( objectType, objectIndex );
}

int vtkExodusIIReader::GetObjectId( int objectType, int objectIndex )
{
  return this->Metadata->GetObjectId( objectType, objectIndex );
}

int vtkExodusIIReader::GetObjectStatus( int objectType, int objectIndex )
{
  return this->Metadata->GetObjectStatus( objectType, objectIndex );
}

void vtkExodusIIReader::SetObjectStatus( int objectType, int objectIndex, int status )
{
  this->Metadata->SetObjectStatus( objectType, objectIndex, status );
}

const char* vtkExodusIIReader::GetObjectName( int objectType, int objectIndex )
{
  return this->Metadata->GetObjectName( objectType, objectIndex );
}

int vtkExodusIIReader::GetObjectIndex( int objectType, const char* objectName )
{
  if ( ! objectName )
    {
    vtkErrorMacro( "You must specify a non-NULL name" );
    return -1;
    }
  int nObj = this->GetNumberOfObjects( objectType );
  if ( nObj == 0 )
    {
    vtkWarningMacro( "No objects of that type (" << objectType << ") to find index for given name " << objectName << "." );
    return -1;
    }
  for ( int obj = 0; obj < nObj; ++obj )
    {
    if ( !strcmp( objectName, this->GetObjectName( objectType, obj ) ) )
      {
      return obj;
      }
    }
  vtkWarningMacro( "No objects named \"" << objectName << "\" of the specified type (" << objectType <<  ")." );
  return -1;
}

int vtkExodusIIReader::GetNumberOfObjectArrays( int objectType )
{
  return this->Metadata->GetNumberOfObjectArraysOfType( objectType );
}

const char* vtkExodusIIReader::GetObjectArrayName( int objectType, int arrayIndex )
{
  return this->Metadata->GetObjectArrayName( objectType, arrayIndex );
}

int vtkExodusIIReader::GetNumberOfObjectArrayComponents( int objectType, int arrayIndex )
{
  return this->Metadata->GetNumberOfObjectArrayComponents( objectType, arrayIndex );
}

int vtkExodusIIReader::GetObjectArrayStatus( int objectType, int arrayIndex )
{
  return this->Metadata->GetObjectArrayStatus( objectType, arrayIndex );
}

void vtkExodusIIReader::SetObjectArrayStatus( int objectType, int arrayIndex, int status )
{
  this->Metadata->SetObjectArrayStatus( objectType, arrayIndex, status );
}

int vtkExodusIIReader::GetNumberOfObjectAttributes( int objectType, int objectIndex )
{
  return this->Metadata->GetNumberOfObjectAttributes( objectType, objectIndex );
}

const char* vtkExodusIIReader::GetObjectAttributeName( int objectType, int objectIndex, int attribIndex )
{
  return this->Metadata->GetObjectAttributeName( objectType, objectIndex, attribIndex );
}

int vtkExodusIIReader::GetObjectAttributeIndex( int objectType, int objectIndex, const char* attribName )
{
  return this->Metadata->GetObjectAttributeIndex( objectType, objectIndex, attribName );
}

int vtkExodusIIReader::GetObjectAttributeStatus( int objectType, int objectIndex, int attribIndex )
{
  return this->Metadata->GetObjectAttributeStatus( objectType, objectIndex, attribIndex );
}

void vtkExodusIIReader::SetObjectAttributeStatus( int objectType, int objectIndex, int attribIndex, int status )
{
  this->Metadata->SetObjectAttributeStatus( objectType, objectIndex, attribIndex, status );
}

int vtkExodusIIReader::GetObjectArrayIndex( int objectType, const char* arrayName )
{
  if ( ! arrayName )
    {
    vtkErrorMacro( "You must specify a non-NULL name" );
    return -1;
    }
  int nObj = this->GetNumberOfObjectArrays( objectType );
  if ( nObj == 0 )
    {
    vtkWarningMacro( "No objects of that type (" << objectType << ") to find index for given array " << arrayName << "." );
    return -1;
    }
  for ( int obj = 0; obj < nObj; ++obj )
    {
    if ( !strcmp( arrayName, this->GetObjectArrayName( objectType, obj ) ) )
      {
      return obj;
      }
    }
  vtkWarningMacro( "No arrays named \"" << arrayName << "\" of the specified type (" << objectType <<  ")." );
  return -1;
}

int vtkExodusIIReader::GetTotalNumberOfNodes() { return this->Metadata->GetModelParams()->num_nodes; }
int vtkExodusIIReader::GetTotalNumberOfEdges() { return this->Metadata->GetModelParams()->num_edge; }
int vtkExodusIIReader::GetTotalNumberOfFaces() { return this->Metadata->GetModelParams()->num_face; }
int vtkExodusIIReader::GetTotalNumberOfElements() { return this->Metadata->GetModelParams()->num_elem; }

// %---------------------------------------------------------------------------
int vtkExodusIIReader::GetNumberOfPartArrays()
{
  return 0;
}

const char* vtkExodusIIReader::GetPartArrayName( int arrayIdx )
{
  (void)arrayIdx;
  return "FIXME";
}

int vtkExodusIIReader::GetPartArrayID( const char *name )
{
  (void)name;
  return 0;
}

const char* vtkExodusIIReader::GetPartBlockInfo( int arrayIdx )
{
  (void)arrayIdx;
  return "FIXME";
}

void vtkExodusIIReader::SetPartArrayStatus( int index, int flag )
{
  (void)index;
  (void)flag;
}

void vtkExodusIIReader::SetPartArrayStatus( const char* part, int flag )
{
  (void)part;
  (void)flag;
}

int vtkExodusIIReader::GetPartArrayStatus( int index )
{
  (void)index;
  return 0;
}

int vtkExodusIIReader::GetPartArrayStatus( const char* part )
{
  (void)part;
  return 0;
}

int vtkExodusIIReader::GetNumberOfMaterialArrays()
{
  return 0;
}

const char* vtkExodusIIReader::GetMaterialArrayName( int arrayIdx )
{
  (void)arrayIdx;
  return "FIXME";
}

int vtkExodusIIReader::GetMaterialArrayID( const char* matl )
{
  (void)matl;
  return 0;
}

void vtkExodusIIReader::SetMaterialArrayStatus( int index, int flag )
{
  (void)index;
  (void)flag;
}

void vtkExodusIIReader::SetMaterialArrayStatus( const char* matl, int flag )
{
  (void)matl;
  (void)flag;
}

int vtkExodusIIReader::GetMaterialArrayStatus( int index )
{
  (void)index;
  return 0;
}

int vtkExodusIIReader::GetMaterialArrayStatus( const char* matl )
{
  (void)matl;
  return 0;
}

int vtkExodusIIReader::GetNumberOfAssemblyArrays()
{
  return 0;
}

const char* vtkExodusIIReader::GetAssemblyArrayName( int arrayIdx )
{
  (void)arrayIdx;
  return "FIXME";
}

int vtkExodusIIReader::GetAssemblyArrayID( const char* name )
{
  (void)name;
  return 0;
}

void vtkExodusIIReader::SetAssemblyArrayStatus( int index, int flag )
{
  (void)index;
  (void)flag;
}

void vtkExodusIIReader::SetAssemblyArrayStatus( const char* name, int flag )
{
  (void)name;
  (void)flag;
}

int vtkExodusIIReader::GetAssemblyArrayStatus( int index )
{
  (void)index;
  return 0;
}

int vtkExodusIIReader::GetAssemblyArrayStatus( const char* name )
{
  (void)name;
  return 0;
}

int vtkExodusIIReader::GetNumberOfHierarchyArrays()
{
  return 0;
}

const char* vtkExodusIIReader::GetHierarchyArrayName( int arrayIdx )
{
  (void)arrayIdx;
  return "FIXME";
}

void vtkExodusIIReader::SetHierarchyArrayStatus( int index, int flag )
{
  (void)index;
  (void)flag;
}

void vtkExodusIIReader::SetHierarchyArrayStatus( const char* name, int flag )
{
  (void)name;
  (void)flag;
}

int vtkExodusIIReader::GetHierarchyArrayStatus( int index )
{
  (void) index;
  return 0;
}

int vtkExodusIIReader::GetHierarchyArrayStatus( const char* name )
{
  (void)name;
  return 0;
}

void vtkExodusIIReader::SetDisplayType( int typ )
{
  if ( typ == this->DisplayType || typ < 0 || typ > 2 )
    return;

  this->DisplayType = typ;
  this->Modified();
}

int vtkExodusIIReader::IsValidVariable( const char *type, const char *name )
{
  return (this->GetVariableID( type, name ) >= 0);
}

int vtkExodusIIReader::GetVariableID( const char *type, const char *name )
{
  int otyp = this->GetObjectTypeFromName( type );
  if ( otyp < 0 )
    {
    return 0;
    }
  switch ( otyp )
    {
  case NODAL:
  case EDGE_BLOCK:
  case FACE_BLOCK:
  case ELEM_BLOCK:
  case NODE_SET:
  case EDGE_SET:
  case FACE_SET:
  case SIDE_SET:
  case ELEM_SET:
    return this->GetObjectArrayIndex( otyp, name );
  case ASSEMBLY:
    return this->GetAssemblyArrayID( name );
  case HIERARCHY:
    return -1; // FIXME: There is no this->GetHierarchyArrayID( name ) and it's not clear there should be.
  case MATERIAL:
    return this->GetMaterialArrayID( name );
  case PART:
    return this->GetPartArrayID( name );
  default:
    return -1;
    }
}

int vtkExodusIIReader::GetTimeSeriesData( int ID, const char* vName, const char* vType, vtkFloatArray* result )
{
  (void)ID;
  (void)vName;
  (void)vType;
  (void)result;
  return -1;
}

// %------------------------------------------------------------- DSP FILTERING
int vtkExodusIIReader::GetNumberOfVariableArrays()
{ //FIXME: Include all time-varying results, not just node and element
  return
    this->GetNumberOfElementResultArrays() +
    this->GetNumberOfPointResultArrays();
}

const char* vtkExodusIIReader::GetVariableArrayName( int aIndex )
{
  int ne = this->GetNumberOfElementResultArrays();
  if ( aIndex < ne )
    {
    return this->GetElementResultArrayName( aIndex );
    }
  return this->GetPointResultArrayName( aIndex - ne );
}

void vtkExodusIIReader::EnableDSPFiltering()
{
  this->DSPFilteringIsEnabled = 1;
  int neb = this->GetNumberOfElementBlockArrays();
  if ( ! this->DSPFilters && neb )
    {
    this->DSPFilters = new vtkDSPFilterGroup*[neb];
    for ( int i = 0; i < neb; ++i )
      {
      this->DSPFilters[i] = vtkDSPFilterGroup::New();
      }
    }
}

void vtkExodusIIReader::AddFilter( vtkDSPFilterDefinition* aFilter )
{
  this->DSPFilteringIsEnabled = 1; // Is this var necessary any more?

  int neb = this->GetNumberOfElementBlockArrays();
  if( ! this->DSPFilters && neb )
    {
    this->DSPFilters = new vtkDSPFilterGroup*[neb];
    for ( int i = 0; i < neb; ++i )
      {
      this->DSPFilters[i] = vtkDSPFilterGroup::New();
      }
    }
  if ( ! this->DSPFilters )
    return;

  for ( int i = 0; i < neb; ++i )
    {
    this->DSPFilters[i]->AddFilter( aFilter );
    }

  this->Modified();
}

void vtkExodusIIReader::StartAddingFilter()
{
  this->AddingFilter->Clear();
}

void vtkExodusIIReader::AddFilterInputVar( char* name )
{
  this->AddingFilter->SetInputVariableName( name );
}

void vtkExodusIIReader::AddFilterOutputVar( char* name )
{
  this->AddingFilter->SetOutputVariableName( name );
}

void vtkExodusIIReader::AddFilterNumeratorWeight( double weight )
{
  this->AddingFilter->PushBackNumeratorWeight( weight );
}

void vtkExodusIIReader::AddFilterForwardNumeratorWeight( double weight )
{
  this->AddingFilter->PushBackForwardNumeratorWeight( weight );
}

void vtkExodusIIReader::AddFilterDenominatorWeight( double weight )
{
  this->AddingFilter->PushBackDenominatorWeight( weight );
}

void vtkExodusIIReader::FinishAddingFilter()
{
  this->AddFilter( this->AddingFilter );
}

void vtkExodusIIReader::RemoveFilter( char* outputVariableName )
{
  int neb = this->GetNumberOfElementBlockArrays();
  if ( ! this->DSPFilters && neb )
    {
    this->DSPFilters = new vtkDSPFilterGroup*[neb];
    for ( int i = 0; i < neb; ++i )
      {
      this->DSPFilters[i] = vtkDSPFilterGroup::New();
      }
    }
  if ( ! this->DSPFilters )
    return;

  for ( int i = 0; i < neb; ++i )
    {
    this->DSPFilters[i]->RemoveFilter( outputVariableName );
    }

  this->Modified();
}

void vtkExodusIIReader::GetDSPOutputArrays( int exoid, int timeStep, vtkUnstructuredGrid* output )
{
  (void)exoid;
  int neb = this->GetNumberOfElementBlockArrays();
  int npr = this->GetNumberOfPointResultArrays();
  int ner = this->GetNumberOfElementResultArrays();
  //int neb = this->GetNumberOfElementBlocks();
  if ( ! this->DSPFilters && neb )
    {
    this->DSPFilters = new vtkDSPFilterGroup*[neb];
    for ( int i = 0; i < neb; ++i )
      {
      this->DSPFilters[i] = vtkDSPFilterGroup::New();
      }
    }

  if ( this->DSPFilteringIsEnabled && this->DSPFilters )
    {
    //printf("in vtkExodusReader::GetDSPOutputArrays DSPFilters IS allocated\n");

    int l_numPointVarInstancesLoaded = 0;
    int l_numCellVarInstancesLoaded = 0;
    int i, j;

    //GET ALL THE INPUTS
    //This is a brute force approach, but will never be problem-sized
    for ( int l_whichVar = 0;
        l_whichVar < this->GetNumberOfVariableArrays();
        ++l_whichVar )
      {
      const char *l_name = this->GetVariableArrayName( l_whichVar );
      for ( int l_whichTime = 0;
            l_whichTime < this->GetNumberOfTimeSteps();
            ++l_whichTime )
        {
        //assuming all blocks' filters have same needs
        int l_needed = 
          this->DSPFilters[0]->IsThisInputVariableInstanceNeeded( l_name, l_whichTime, timeStep );
        if ( l_needed ) 
          {
          // cannot assume all blocks' filters have the same cache,
          // because a block may have been turned off before
          for ( i = 0; i < neb; ++i )
            {
            //XXX STILL NEED TO HANDLE TRUTH TABLES FOR SHIP.EXO
            if ( this->GetElementBlockArrayStatus( i ) )
              {
              int l_cached = 
                this->DSPFilters[i]->IsThisInputVariableInstanceCached( l_name, l_whichTime );
              if ( ! l_cached )
                {
                //Get the type of var, and the index of var
                int l_varIndex = -1;
                int l_pointArrayIndex = 0, l_cellArrayIndex = 0; //this is confusing
                int l_isPointArray = 0;
                for ( j = 0; j < npr; ++j )
                  {
                  if ( ! strcmp( this->GetPointResultArrayName( j ), l_name ) )
                    {
                    l_varIndex = j;
                    l_isPointArray = 1;
                    break;
                    }
                  l_pointArrayIndex += this->GetPointResultArrayNumberOfComponents( j );
                  }
                for ( j = 0; j < ner; ++j )
                  {
                  if ( ! strcmp( this->GetElementResultArrayName( j ), l_name ) )
                    {
                    if ( l_varIndex >= 0 )
                      {           
                      vtkErrorMacro( "Apparently there are nodal and element block result variables with same name: "  << l_name );
                      break;
                      }
                    l_varIndex = j;
                    l_isPointArray = 0;
                    break;
                    }
                  l_cellArrayIndex += this->GetElementResultArrayNumberOfComponents( j );
                  }
                if ( l_varIndex < 0 )
                  {
                  vtkErrorMacro( "Can't find nodal or element block vars with name: " << l_name );
                  break;
                  }


                if ( l_isPointArray )
                  {
                  // Can't use l_varIndex here, because the output's 'Point Data' may
                  // not have all the vars that the actual input Point Data has
                  vtkDataArray* l_array = output->GetPointData()->GetArray( l_name );
        
                  if ( ! l_array )
                    {
                    //FIXME: Is NODAL the global version with "squeezed" points? If not, change this...
                    l_array = this->Metadata->GetCacheOrRead( vtkExodusIICacheKey( l_whichTime, NODAL, 0, l_pointArrayIndex ) );
                    }

                  if ( ! l_array )
                    {           
                    vtkErrorMacro( "Can't get point array: " << l_name );
                    break;
                    }

                  if ( ! l_array->GetNumberOfComponents() || !l_array->GetNumberOfTuples() )
                    {
                    vtkErrorMacro( "Zero sized point array: " << l_name );
                    break;
                    }

                  int l_type = l_array->GetDataType();
                  if ( l_type != VTK_FLOAT ) 
                    {
                    vtkWarningMacro( "vtkExodusReader::GetDSPOutputArrays can only do floats for now (type=" << l_type << ")" );
                    }
                  else
                    {
                    vtkFloatArray *l_floatArray = static_cast<vtkFloatArray *>( l_array );
                    this->DSPFilters[i]->AddInputVariableInstance( l_name,l_whichTime, l_floatArray );

                    l_numPointVarInstancesLoaded++;
                    }

                  //l_array->Delete(); //DONT DELETE HERE 26aug

                  /*END OF POINT ARRAY PART*/ 
                  }
                else
                  {
                  // BEGIN CELL ARRAY PART
                  // Can't use l_varIndex here, because the output's 'Point Data' may
                  // not have all the vars that the actual input Point Data has
                  vtkDataArray *l_array = output->GetCellData()->GetArray( l_name );
                  if ( ! l_array )
                    {
                    l_array = this->Metadata->GetCacheOrRead(
                      vtkExodusIICacheKey( l_whichTime, GLOBAL, ELEM_BLOCK, l_cellArrayIndex ) );
                    }

                  if ( !l_array )
                    {           
                    vtkErrorMacro( "Can't get cell array: " << l_name );
                    break;
                    }

                  if ( ! l_array->GetNumberOfComponents() || ! l_array->GetNumberOfTuples() )
                    {
                    vtkErrorMacro( "Zero sized cell array: " << l_name );
                    break;
                    }         


                  int l_type = l_array->GetDataType();
                  if ( l_type != VTK_FLOAT ) 
                    {
                    vtkWarningMacro( "vtkExodusReader::GetDSPOutputArrays can only do floats for now (type=" << l_type << ")" );
                    }
                  else
                    {
                    vtkFloatArray* l_floatArray = static_cast<vtkFloatArray *>( l_array );
                    this->DSPFilters[i]->AddInputVariableInstance( l_name, l_whichTime, l_floatArray );

                    ++l_numCellVarInstancesLoaded;
                    }

                  //l_array->Delete(); //DONT DELETE HERE 26aug

                  /*END OF CELL ARRAY PART*/

                  }
                }
              //else printf("...vtkExodusReader DSP FILTERING not loading time %d %s for block %d of %d ALREADY CACHED\n",l_whichTime,l_name,i,neb);

              }
            //else printf("...vtkExodusReader DSP FILTERING time %d %s for block %d of %d HAS 0 STATUS\n",l_whichTime,l_name,i,neb);
            }
          }
        //else  printf("...vtkExodusReader DSP FILTERING time %d %s NOT NEEDED\n",l_whichTime,l_name);
        }
      }
    printf("vtkExodusReader::GetDSPOutputArrays() read %d dsp POINT "
           "input variable instances\n",l_numPointVarInstancesLoaded);
    printf("vtkExodusReader::GetDSPOutputArrays() read %d dsp CELL "
           "input variable instances\n",l_numCellVarInstancesLoaded);
    
    //CALCULATE THE OUTPUTS
    int l_numCalculated = 0;
    int l_numFilters = (int) this->DSPFilters[0]->GetNumFilters();
    for ( i = 0; i < neb; ++i )
      {
      for ( j = 0; j < l_numFilters; ++j)
        {
        //Figure out whether the input (and therefore output) var
        //is cell or point
        int l_isCellVar = 1;
        int l_var = -1;
        for ( l_var = 0; l_var < npr; ++l_var )
          {
          if ( ! strcmp( this->GetPointResultArrayName(l_var), this->DSPFilters[i]->GetInputVariableName( j ) ) )
            {
            l_isCellVar = 0;
            break;
            } 
          }

        vtkFloatArray* l_array = 0;
        if( this->GetElementBlockArrayStatus( i ) )
          {
          l_array = this->DSPFilters[i]->GetOutput( j, timeStep, l_numCalculated );
          }

        if(l_array)
          {
#if 0 //get the min/max and print it out
          int l_datatype = l_array->GetDataType();
          if( l_datatype == VTK_FLOAT ) 
            {
            for ( int l_comp=0; l_comp < l_array->GetNumberOfComponents(); ++l_comp )
              {
              float l_min,l_max;
              float* l_data = (float*)l_array->GetVoidPointer( 0 );
        
              for ( int l_tup = 0; l_tup < l_array->GetNumberOfTuples(); ++l_tup )
                {
                float l_val = l_data[l_tup * l_array->GetNumberOfComponents() + l_comp];
                if ( ! l_tup )
                  {
                  l_min = l_max = l_val;
                  }
                else if ( l_min > l_val )
                  {
                  l_min = l_val;
                  }
                else if ( l_max < l_val )
                  {
                  l_max = l_val;
                  }
                }
              printf( "*****************comp=%d  min=%f max=%f\n", l_comp, l_min, l_max );
              }
            }

#endif

          if( ! l_isCellVar )
            {
            output->GetPointData()->AddArray( l_array );
            //printf("added dsp point var block %d\n",i);
            }
          else
            {
            output->GetCellData()->AddArray( l_array );
            //printf("added dsp cell var block %d\n",i);
            }
          }

        //printf("block %d AFTER DSP  num pt arrays = %d, num cell arrays = %d    num pts=%d  num cells=%d\n", i,
        // output->GetPointData()->GetNumberOfArrays(), output->GetCellData()->GetNumberOfArrays(),
        // output->GetPoints()->GetNumberOfPoints(), output->GetNumberOfCells() );

        }
      }

    printf("---vtkExodusReader::GetDSPOutputArrays() calculated %d dsp output "
           "variable instances---\n",l_numCalculated);
    }
}

// %---------------------------------------------------------------------------

void vtkExodusIIReader::SetAllArrayStatus( int otyp, int status )
{
  int numObj;
  int i;
  switch ( otyp )
    {
  case EDGE_BLOCK_CONN:
  case FACE_BLOCK_CONN:
  case ELEM_BLOCK_ELEM_CONN:
  case NODE_SET_CONN:
  case EDGE_SET_CONN:
  case FACE_SET_CONN:
  case SIDE_SET_CONN:
  case ELEM_SET_CONN:
    numObj = this->GetNumberOfObjects( otyp );
    for ( i = 0; i < numObj; ++i )
      {
      this->SetObjectStatus( otyp, i, status );
      }
    break;
  case NODAL:
  case EDGE_BLOCK:
  case FACE_BLOCK:
  case ELEM_BLOCK:
  case NODE_SET:
  case EDGE_SET:
  case FACE_SET:
  case SIDE_SET:
  case ELEM_SET:
    numObj = this->GetNumberOfObjectArrays( otyp );
    for ( i = 0; i < numObj; ++i )
      {
      this->SetObjectArrayStatus( otyp, i, status );
      }
    break;
  // ---------------------
  case ASSEMBLY:
    numObj = this->GetNumberOfAssemblyArrays();
    for ( i = 0; i < numObj; ++i )
      {
      this->SetAssemblyArrayStatus( i, status );
      }
  case PART:
    numObj = this->GetNumberOfPartArrays();
    for ( i = 0; i < numObj; ++i )
      {
      this->SetPartArrayStatus( i, status );
      }
  case MATERIAL:
    numObj = this->GetNumberOfMaterialArrays();
    for ( i = 0; i < numObj; ++i )
      {
      this->SetMaterialArrayStatus( i, status );
      }
  case HIERARCHY:
    numObj = this->GetNumberOfHierarchyArrays();
    for ( i = 0; i < numObj; ++i )
      {
      this->SetHierarchyArrayStatus( i, status );
      }
  default:
    ;
    break;
    }
}

void vtkExodusIIReader::NewExodusModel()
{
  // These arrays are required by the Exodus II writer:
  this->GenerateGlobalElementIdArrayOn();
  this->GenerateGlobalNodeIdArrayOn();
  this->GenerateObjectIdCellArrayOn();

  if ( this->ExodusModel )
    {
    this->ExodusModel->Reset();
    return;
    }

  this->ExodusModel = vtkExodusModel::New();
}
