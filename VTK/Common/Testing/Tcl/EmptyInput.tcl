for {set i  0} {$i < [expr $argc - 1]} {incr i} {
   if {[lindex $argv $i] == "-A"} {
      set auto_path "$auto_path [lindex $argv [expr $i +1]]"
   }
}

package require vtktcl
vtkObject a
a GlobalWarningDisplayOff
a Delete


vtkPolyData emptyPD
vtkImageData emptyID
vtkStructuredGrid emptySG
vtkUnstructuredGrid emptyUG
vtkRectilinearGrid emptyRG

proc TestOne {cname} {

   $cname b 

    if {[b IsA "vtkSource"]} {
	catch {b SetInput emptyPD}
	catch {b Update}
	catch {b SetInput emptyID}
	catch {b Update}
	catch {b SetInput emptySG}
	catch {b Update}
	catch {b SetInput emptyUG}
	catch {b Update}
	catch {b SetInput emptyRG}
	catch {b Update}
    }

   b Delete
}

set classExceptions {
    vtkCommand
    vtkIndent
    vtkTimeStamp
    vtkTkImageViewerWidget
    vtkTkImageWindowWidget
    vtkTkRenderWidget
    vtkJPEGReader
}

proc rtEmptyInputTest { fileid } { 
   global classExceptions
   # for every class
   set all [lsort [info command vtk*]]
   foreach a $all {
      if {[lsearch $classExceptions $a] == -1} {
         # test some set get methods
         #puts "Testing -- $a"
         TestOne $a
      }
   }
}

# All tests should end with the following...

rtEmptyInputTest stdout

exit
