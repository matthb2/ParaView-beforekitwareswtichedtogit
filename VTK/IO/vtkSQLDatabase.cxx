/*=========================================================================

Program:   Visualization Toolkit
Module:    $RCSfile$

Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
All rights reserved.
See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/*----------------------------------------------------------------------------
  Copyright (c) Sandia Corporation
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.
  ----------------------------------------------------------------------------*/

#include "vtkToolkits.h"
#include "vtkSQLDatabase.h"
#include "vtkSQLQuery.h"

#include "vtkSQLDatabaseSchema.h"

#include "vtkSQLiteDatabase.h"

#ifdef VTK_USE_POSTGRES
#include "vtkPostgreSQLDatabase.h"
#endif // VTK_USE_POSTGRES

#ifdef VTK_USE_MYSQL
#include "vtkMySQLDatabase.h"
#endif // VTK_USE_MYSQL

#include "vtkObjectFactory.h"
#include "vtkStdString.h"

#include <vtksys/SystemTools.hxx>

vtkCxxRevisionMacro(vtkSQLDatabase, "$Revision$");

// ----------------------------------------------------------------------
vtkSQLDatabase::vtkSQLDatabase()
{
}

// ----------------------------------------------------------------------
vtkSQLDatabase::~vtkSQLDatabase()
{
}

// ----------------------------------------------------------------------
void vtkSQLDatabase::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

// ----------------------------------------------------------------------
vtkStdString vtkSQLDatabase::GetColumnSpecification( vtkSQLDatabaseSchema* schema,
                                                     int tblHandle,
                                                     int colHandle )
{
  vtkStdString queryStr = schema->GetColumnNameFromHandle( tblHandle, colHandle );

  int colType = schema->GetColumnTypeFromHandle( tblHandle, colHandle ); 

  vtkStdString colTypeStr = this->GetColumnTypeString( colType );
  if ( colTypeStr )
    {
    queryStr += " ";
    queryStr += colTypeStr;
    }
  else // if ( colTypeStr )
    {
    vtkGenericWarningMacro( "Unable to get column specification: unsupported data type " << colType );
    return 0;
    }
  
  vtkStdString attStr = schema->GetColumnAttributesFromHandle( tblHandle, colHandle );
  if ( attStr )
    {
    queryStr += " ";
    queryStr += attStr;
    }

  return queryStr;
}

// ----------------------------------------------------------------------
vtkStdString vtkSQLDatabase::GetIndexSpecification( vtkSQLDatabaseSchema* schema,
                                                    int tblHandle,
                                                    int idxHandle )
{
  vtkStdString queryStr = ", ";

  int idxType = schema->GetIndexTypeFromHandle( tblHandle, idxHandle );
  switch ( idxType )
    {
    case vtkSQLDatabaseSchema::PRIMARY_KEY:
      queryStr += "PRIMARY KEY ";
      break;
    case vtkSQLDatabaseSchema::UNIQUE: // Not supported by all SQL backends
      return 0;
    case vtkSQLDatabaseSchema::INDEX:
      queryStr += "INDEX ";
      break;
    default:
      return 0;
    }
  
  queryStr += schema->GetIndexNameFromHandle( tblHandle, idxHandle );
  queryStr += " (";
        
  // Loop over all column names of the index
  int numCnm = schema->GetNumberOfColumnNamesInIndex( tblHandle, idxHandle );
  if ( numCnm < 0 )
    {
    vtkGenericWarningMacro( "Unable to get index specification: index has incorrect number of columns " << numCnm );
    return 0;
    }

  bool firstCnm = true;
  for ( int cnmHandle = 0; cnmHandle < numCnm; ++ cnmHandle )
    {
    if ( firstCnm )
      {
      firstCnm = false;
      }
    else
      {
      queryStr += ",";
      }
    queryStr += schema->GetIndexColumnNameFromHandle( tblHandle, idxHandle, cnmHandle );
    }
  queryStr += ")";

  return queryStr;
}

// ----------------------------------------------------------------------
vtkStdString vtkSQLDatabase::GetColumnTypeString( int colType )
{
  switch ( static_cast<vtkSQLDatabaseSchema::DatabaseColumnType>( colType ) )
    {
    case vtkSQLDatabaseSchema::SERIAL: return 0;
    case vtkSQLDatabaseSchema::SMALLINT: return "INTEGER";
    case vtkSQLDatabaseSchema::INTEGER: return "INTEGER";
    case vtkSQLDatabaseSchema::BIGINT: return "INTEGER";
    case vtkSQLDatabaseSchema::VARCHAR: return "VARCHAR";
    case vtkSQLDatabaseSchema::TEXT: return 0;
    case vtkSQLDatabaseSchema::REAL: return "DOUBLE";
    case vtkSQLDatabaseSchema::DOUBLE: return "DOUBLE";
    case vtkSQLDatabaseSchema::BLOB: return 0;
    case vtkSQLDatabaseSchema::TIME: return "TIME";
    case vtkSQLDatabaseSchema::DATE: return "DATE";
    case vtkSQLDatabaseSchema::TIMESTAMP: return "TIMESTAMP";
    }

  return 0;
}

// ----------------------------------------------------------------------
vtkSQLDatabase* vtkSQLDatabase::CreateFromURL( const char* URL )
{
  vtkstd::string protocol;
  vtkstd::string username; 
  vtkstd::string password;
  vtkstd::string hostname; 
  vtkstd::string dataport; 
  vtkstd::string database;
  vtkstd::string dataglom;
  vtkSQLDatabase* db = 0;
  
  // SQLite is a bit special so lets get that out of the way :)
  if ( ! vtksys::SystemTools::ParseURLProtocol( URL, protocol, dataglom))
    {
    vtkGenericWarningMacro( "Invalid URL: " << URL );
    return 0;
    }
  if ( protocol == "sqlite" )
    {
    db = vtkSQLiteDatabase::New();
    vtkSQLiteDatabase *sqlite_db = vtkSQLiteDatabase::SafeDownCast(db);
    sqlite_db->SetDatabaseFileName(dataglom.c_str());
    return db;
    }
    
  // Okay now for all the other database types get more detailed info
  if ( ! vtksys::SystemTools::ParseURL( URL, protocol, username,
                                        password, hostname, dataport, database) )
    {
    vtkGenericWarningMacro( "Invalid URL: " << URL );
    return 0;
    }
  
#ifdef VTK_USE_POSTGRES
  if ( protocol == "psql" )
    {
    db = vtkPostgreSQLDatabase::New();
    vtkPostgreSQLDatabase *post_db = vtkPostgreSQLDatabase::SafeDownCast(db);
    post_db->SetUserName(username.c_str());
    post_db->SetPassword(password.c_str());
    post_db->SetHostName(hostname.c_str());
    post_db->SetServerPort(atoi(dataport.c_str()));
    post_db->SetDatabaseName(database.c_str());
    return db;
    }
#endif // VTK_USE_POSTGRES
#ifdef VTK_USE_MYSQL
  if ( protocol == "mysql" )
    {
    db = vtkMySQLDatabase::New();
    vtkMySQLDatabase *mysql_db = vtkMySQLDatabase::SafeDownCast(db);
    if ( username.size() )
      {
      mysql_db->SetUserName(username.c_str());
      }
    if ( password.size() )
      {
      mysql_db->SetPassword(password.c_str());
      }
    if ( dataport.size() )
      {
      mysql_db->SetServerPort(atoi(dataport.c_str()));
      }
    mysql_db->SetHostName(hostname.c_str());
    mysql_db->SetDatabaseName(database.c_str());
    return db;
    }
#endif // VTK_USE_MYSQL

  vtkGenericWarningMacro( "Unsupported protocol: " << protocol.c_str() );
  return db;
}

// ----------------------------------------------------------------------
bool vtkSQLDatabase::EffectSchema( vtkSQLDatabaseSchema* schema, bool vtkNotUsed(dropIfExists) )
{
  if ( ! this->IsOpen() )
    {
    vtkGenericWarningMacro( "Unable to effect the schema: no database is open" );
    return false;
    }

  // Instantiate an empty query and begin the transaction.
  vtkSQLQuery* query = this->GetQueryInstance();
  if ( ! query->BeginTransaction() )
    {
    vtkGenericWarningMacro( "Unable to effect the schema: unable to begin transaction" );
    return false;
    }
 
  // Loop over all tables of the schema and create them
  int numTbl = schema->GetNumberOfTables();
  for ( int tblHandle = 0; tblHandle < numTbl; ++ tblHandle )
    {
    // Construct the query string for this table
    vtkStdString queryStr( "CREATE TABLE " );
    queryStr += schema->GetTableNameFromHandle( tblHandle );
    queryStr += " (";

    // Loop over all columns of the current table
    int numCol = schema->GetNumberOfColumnsInTable( tblHandle );
    if ( numCol < 0 )
      {
      query->RollbackTransaction();
      return false;
      }

    bool firstCol = true;
    for ( int colHandle = 0; colHandle < numCol; ++ colHandle )
      {
      if ( ! firstCol )
        {
        queryStr += ", ";
        }
      else // ( ! firstCol )
        {
        firstCol = false;
        }

      // Get column creation syntax (backend-dependent)
      vtkStdString colStr = this->GetColumnSpecification( schema, tblHandle, colHandle );
      if ( colStr )
        {
        queryStr += colStr;
        }
      else // if ( colStr )
        {
        query->RollbackTransaction();
        return false;
        }
      }

    // Loop over all indices of the current table
    int numIdx = schema->GetNumberOfIndicesInTable( tblHandle );
    if ( numIdx < 0 )
      {
      query->RollbackTransaction();
      return false;
      }
    for ( int idxHandle = 0; idxHandle < numIdx; ++ idxHandle )
      {
      // Get index creation syntax (backend-dependent)
      vtkStdString idxStr = this->GetIndexSpecification( schema, tblHandle, idxHandle );
      if ( idxStr )
        {
        queryStr += idxStr;
        }
      else // if ( idxStr )
        {
        query->RollbackTransaction();
        return false;
        }

      }
    queryStr += ")";

    // Execute the query
    query->SetQuery( queryStr );
    if ( ! query->Execute() )
      {
      vtkGenericWarningMacro( "Unable to effect the schema: unable to execute query.\nDetails: "
                              << query->GetLastErrorText() );
      query->RollbackTransaction();
      return false;
      }
    }
  
  // FIXME: eventually handle triggers

  // Commit the transaction.
  if ( ! query->CommitTransaction() )
    {
    vtkGenericWarningMacro( "Unable to effect the schema: unable to commit transaction.\nDetails: "
                            << query->GetLastErrorText() );
    return false;
    }

  return true;
}
