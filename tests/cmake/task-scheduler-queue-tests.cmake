if (TARGET zr_vm_task_job_scheduler_test)
    add_test(NAME task_job_scheduler COMMAND zr_vm_task_job_scheduler_test)
    set_tests_properties(task_job_scheduler PROPERTIES
            LABELS "ssa;task"
            TIMEOUT 120)
endif ()
