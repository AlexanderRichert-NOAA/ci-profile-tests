! Minimal Fortran program: compute and print 1+2+...+N.
program sum
    implicit none
    integer :: n, i
    integer(kind=8) :: s

    call get_argument(n)

    s = 0
    do i = 1, n
        s = s + i
    end do

    write(*, '(a,i0,a,i0)') 'n=', n, '  sum=', s

contains

    subroutine get_argument(val)
        integer, intent(out) :: val
        character(len=32) :: arg
        if (command_argument_count() /= 1) then
            write(*, '(a)') 'Usage: sum_fortran <N>'
            stop 1
        end if
        call get_command_argument(1, arg)
        read(arg, *) val
        if (val <= 0) then
            write(*, '(a)') 'Error: N must be a positive integer'
            stop 1
        end if
    end subroutine get_argument

end program sum
