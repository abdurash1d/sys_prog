/**
 * CPU and Memory Usage Tracker with Kill Switch
 * 
 * Cross-platform application to monitor CPU and memory usage
 * of running processes with the ability to terminate them.
 */

 #include <gtk/gtk.h>
 #include <pthread.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 
 #ifdef _WIN32
 #include <windows.h>
 #include <psapi.h>
 #include <tlhelp32.h>
 #elif defined(__APPLE__)
 #include <sys/sysctl.h>
 #include <sys/types.h>
 #include <mach/mach.h>
 #include <mach/task.h>
 #include <mach/mach_init.h>
 #include <signal.h>
 #else /* Linux */
 #include <dirent.h>
 #include <signal.h>
 #include <sys/types.h>
 #endif
 
 /* Type definitions */
 typedef struct {
     int pid;
     char name[256];
     double cpu_usage;
     double memory_usage;
 } ProcessInfo;
 
 typedef struct {
     ProcessInfo *processes;
     int count;
     int capacity;
 } ProcessList;
 
 /* Global variables */
 GtkWidget *process_view;
 GtkListStore *process_store;
 ProcessList process_list;
 pthread_t update_thread;
 pthread_mutex_t data_mutex;
 int update_interval_ms = 2000;  // Default update interval
 gboolean running = TRUE;
 
 /* Function prototypes */
 static void init_process_list(ProcessList *list);
 static void clear_process_list(ProcessList *list);
 static void add_process(ProcessList *list, int pid, const char *name, double cpu, double mem);
 static void update_process_data();
 static void *update_thread_func(void *data);
 static gboolean populate_process_view(gpointer data);  // Changed return type to gboolean
 static void kill_process(int pid);
 static void on_kill_button_clicked(GtkWidget *widget, gpointer data);
 static void on_refresh_button_clicked(GtkWidget *widget, gpointer data);
 static void on_interval_changed(GtkSpinButton *spinbutton, gpointer data);
 static void on_row_selected(GtkTreeSelection *selection, gpointer data);
 
 /* Platform-specific functions */
 #ifdef _WIN32
 static void get_win_processes(ProcessList *list);
 #elif defined(__APPLE__)
 static void get_mac_processes(ProcessList *list);
 #else
 static void get_linux_processes(ProcessList *list);
 #endif
 
 int main(int argc, char *argv[]) {
     /* Initialize GTK */
     gtk_init(&argc, &argv);
     
     /* Initialize process list and mutex */
     init_process_list(&process_list);
     pthread_mutex_init(&data_mutex, NULL);
     
     /* Create the main window */
     GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
     gtk_window_set_title(GTK_WINDOW(window), "CPU & Memory Usage Tracker");
     gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
     g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
     
     /* Create the main layout container */
     GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
     gtk_container_add(GTK_CONTAINER(window), main_box);
     
     /* Create the control panel */
     GtkWidget *control_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
     gtk_box_pack_start(GTK_BOX(main_box), control_box, FALSE, FALSE, 5);
     
     /* Refresh button */
     GtkWidget *refresh_btn = gtk_button_new_with_label("Refresh Now");
     g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_refresh_button_clicked), NULL);
     gtk_box_pack_start(GTK_BOX(control_box), refresh_btn, FALSE, FALSE, 5);
     
     /* Refresh interval */
     GtkWidget *interval_label = gtk_label_new("Refresh Interval (ms):");
     gtk_box_pack_start(GTK_BOX(control_box), interval_label, FALSE, FALSE, 5);
     
     GtkWidget *interval_spin = gtk_spin_button_new_with_range(500, 10000, 100);
     gtk_spin_button_set_value(GTK_SPIN_BUTTON(interval_spin), update_interval_ms);
     g_signal_connect(interval_spin, "value-changed", G_CALLBACK(on_interval_changed), NULL);
     gtk_box_pack_start(GTK_BOX(control_box), interval_spin, FALSE, FALSE, 5);
     
     /* Kill button */
     GtkWidget *kill_btn = gtk_button_new_with_label("Terminate Process");
     gtk_widget_set_sensitive(kill_btn, FALSE);  // Disabled until a process is selected
     g_signal_connect(kill_btn, "clicked", G_CALLBACK(on_kill_button_clicked), NULL);
     gtk_box_pack_end(GTK_BOX(control_box), kill_btn, FALSE, FALSE, 5);
     
     /* Create the process list view */
     GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
     gtk_box_pack_start(GTK_BOX(main_box), scroll, TRUE, TRUE, 0);
     
     /* Create the list store model */
     process_store = gtk_list_store_new(4,
                                       G_TYPE_INT,      // PID
                                       G_TYPE_STRING,   // Process name
                                       G_TYPE_DOUBLE,   // CPU usage %
                                       G_TYPE_DOUBLE);  // Memory usage %
     
     /* Create the tree view */
     process_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(process_store));
     
     /* Add columns */
     GtkCellRenderer *renderer;
     GtkTreeViewColumn *column;
     
     renderer = gtk_cell_renderer_text_new();
     column = gtk_tree_view_column_new_with_attributes("PID",
                                                      renderer,
                                                      "text", 0,
                                                      NULL);
     gtk_tree_view_append_column(GTK_TREE_VIEW(process_view), column);
     
     renderer = gtk_cell_renderer_text_new();
     column = gtk_tree_view_column_new_with_attributes("Process Name",
                                                      renderer,
                                                      "text", 1,
                                                      NULL);
     gtk_tree_view_column_set_expand(column, TRUE);
     gtk_tree_view_append_column(GTK_TREE_VIEW(process_view), column);
     
     renderer = gtk_cell_renderer_text_new();
     column = gtk_tree_view_column_new_with_attributes("CPU %",
                                                      renderer,
                                                      "text", 2,
                                                      NULL);
     gtk_tree_view_append_column(GTK_TREE_VIEW(process_view), column);
     
     renderer = gtk_cell_renderer_text_new();
     column = gtk_tree_view_column_new_with_attributes("Memory %",
                                                      renderer,
                                                      "text", 3,
                                                      NULL);
     gtk_tree_view_append_column(GTK_TREE_VIEW(process_view), column);
     
     /* Set up row selection */
     GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(process_view));
     gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
     g_signal_connect(selection, "changed", G_CALLBACK(on_row_selected), kill_btn);
     
     gtk_container_add(GTK_CONTAINER(scroll), process_view);
     
     /* Show all widgets */
     gtk_widget_show_all(window);
     
     /* Start the update thread */
     pthread_create(&update_thread, NULL, update_thread_func, NULL);
     
     /* Start the GTK main loop */
     gtk_main();
     
     /* Cleanup */
     running = FALSE;
     pthread_join(update_thread, NULL);
     pthread_mutex_destroy(&data_mutex);
     clear_process_list(&process_list);
     
     return 0;
 }
 
 /* Initialize the process list */
 static void init_process_list(ProcessList *list) {
     list->capacity = 100;
     list->count = 0;
     list->processes = (ProcessInfo*)malloc(list->capacity * sizeof(ProcessInfo));
 }
 
 /* Clear the process list */
 static void clear_process_list(ProcessList *list) {
     free(list->processes);
     list->processes = NULL;
     list->count = 0;
     list->capacity = 0;
 }
 
 /* Add a process to the list */
 static void add_process(ProcessList *list, int pid, const char *name, double cpu, double mem) {
     if (list->count >= list->capacity) {
         list->capacity *= 2;
         list->processes = (ProcessInfo*)realloc(list->processes, list->capacity * sizeof(ProcessInfo));
     }
     
     list->processes[list->count].pid = pid;
     strncpy(list->processes[list->count].name, name, 254);
     list->processes[list->count].name[254] = '\0';
     list->processes[list->count].cpu_usage = cpu;
     list->processes[list->count].memory_usage = mem;
     list->count++;
 }
 
 /* Update process data */
 static void update_process_data() {
     pthread_mutex_lock(&data_mutex);
     
     /* Clear the previous list */
     process_list.count = 0;
     
     /* Get platform-specific process data */
 #ifdef _WIN32
     get_win_processes(&process_list);
 #elif defined(__APPLE__)
     get_mac_processes(&process_list);
 #else
     get_linux_processes(&process_list);
 #endif
     
     pthread_mutex_unlock(&data_mutex);
 }
 
 /* Update thread function */
 static void *update_thread_func(void *data) {
     (void)data;  // Unused parameter
     
     while (running) {
         update_process_data();
         
         /* Update the UI from the main thread */
         gdk_threads_add_idle(populate_process_view, process_store);
         
         /* Sleep for the update interval */
         usleep(update_interval_ms * 1000);
     }
     
     return NULL;
 }
 
 /* Populate the process view with data */
 static gboolean populate_process_view(gpointer data) {
     GtkListStore *store = GTK_LIST_STORE(data);
     
     /* Clear the store */
     gtk_list_store_clear(store);
     
     /* Lock the data mutex */
     pthread_mutex_lock(&data_mutex);
     
     /* Add each process to the store */
     for (int i = 0; i < process_list.count; i++) {
         GtkTreeIter iter;
         gtk_list_store_append(store, &iter);
         gtk_list_store_set(store, &iter,
                           0, process_list.processes[i].pid,
                           1, process_list.processes[i].name,
                           2, process_list.processes[i].cpu_usage,
                           3, process_list.processes[i].memory_usage,
                           -1);
     }
     
     /* Unlock the data mutex */
     pthread_mutex_unlock(&data_mutex);
     
     return FALSE;  // Remove the idle source
 }
 
 /* Kill the selected process */
 static void kill_process(int pid) {
 #ifdef _WIN32
     HANDLE handle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
     if (handle != NULL) {
         TerminateProcess(handle, 0);
         CloseHandle(handle);
     }
 #else
     kill(pid, SIGTERM);
 #endif
 }
 
 /* Kill button click handler */
 static void on_kill_button_clicked(GtkWidget *widget, gpointer data) {
     (void)widget;  // Unused parameter
     (void)data;    // Unused parameter
     
     GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(process_view));
     GtkTreeIter iter;
     GtkTreeModel *model;
     
     if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
         int pid;
         gtk_tree_model_get(model, &iter, 0, &pid, -1);
         
         /* Confirm before killing */
         GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_widget_get_toplevel(process_view)),
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_QUESTION,
                                                   GTK_BUTTONS_YES_NO,
                                                   "Are you sure you want to terminate process %d?", pid);
         
         int response = gtk_dialog_run(GTK_DIALOG(dialog));
         gtk_widget_destroy(dialog);
         
         if (response == GTK_RESPONSE_YES) {
             kill_process(pid);
             update_process_data();
             populate_process_view(process_store);
         }
     }
 }
 
 /* Refresh button click handler */
 static void on_refresh_button_clicked(GtkWidget *widget, gpointer data) {
     (void)widget;  // Unused parameter
     (void)data;    // Unused parameter
     
     update_process_data();
     populate_process_view(process_store);
 }
 
 /* Interval change handler */
 static void on_interval_changed(GtkSpinButton *spinbutton, gpointer data) {
     (void)data;  // Unused parameter
     
     update_interval_ms = gtk_spin_button_get_value_as_int(spinbutton);
 }
 
 /* Row selection handler */
 static void on_row_selected(GtkTreeSelection *selection, gpointer data) {
     GtkWidget *kill_btn = GTK_WIDGET(data);
     GtkTreeIter iter;
     GtkTreeModel *model;
     
     if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
         gtk_widget_set_sensitive(kill_btn, TRUE);
     } else {
         gtk_widget_set_sensitive(kill_btn, FALSE);
     }
 }
 
 #ifdef _WIN32
 /* Get Windows processes */
 static void get_win_processes(ProcessList *list) {
     HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
     if (snapshot == INVALID_HANDLE_VALUE) {
         return;
     }
     
     PROCESSENTRY32 process;
     process.dwSize = sizeof(PROCESSENTRY32);
     
     if (!Process32First(snapshot, &process)) {
         CloseHandle(snapshot);
         return;
     }
     
     do {
         HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process.th32ProcessID);
         if (handle) {
             PROCESS_MEMORY_COUNTERS pmc;
             if (GetProcessMemoryInfo(handle, &pmc, sizeof(pmc))) {
                 // Get system memory info for percentage calculation
                 MEMORYSTATUSEX memInfo;
                 memInfo.dwLength = sizeof(MEMORYSTATUSEX);
                 GlobalMemoryStatusEx(&memInfo);
                 
                 // Calculate memory usage percentage
                 double memUsage = (double)pmc.WorkingSetSize / (double)memInfo.ullTotalPhys * 100.0;
                 
                 // TODO: Calculate CPU usage (requires multiple sampling)
                 double cpuUsage = 0.0;  // Placeholder
                 
                 add_process(list, process.th32ProcessID, process.szExeFile, cpuUsage, memUsage);
             }
             CloseHandle(handle);
         }
     } while (Process32Next(snapshot, &process));
     
     CloseHandle(snapshot);
 }
 #elif defined(__APPLE__)
 /* Get macOS processes */
 static void get_mac_processes(ProcessList *list) {
     // Get all BSD processes
     int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
     size_t size;
     
     if (sysctl(mib, 4, NULL, &size, NULL, 0) < 0) {
         return;
     }
     
     struct kinfo_proc *processes = malloc(size);
     if (processes == NULL) {
         return;
     }
     
     if (sysctl(mib, 4, processes, &size, NULL, 0) < 0) {
         free(processes);
         return;
     }
     
     // Calculate number of processes
     int count = size / sizeof(struct kinfo_proc);
     
     // Get host port for memory info
     host_t host = mach_host_self();
     vm_size_t page_size;
     mach_msg_type_number_t count_info;
     vm_statistics_data_t vm_stats;
     mach_port_t host_port;
     
     host_port = mach_host_self();
     count_info = HOST_VM_INFO_COUNT;
     
     if (host_page_size(host, &page_size) != KERN_SUCCESS) {
         page_size = 4096;  // Default page size if we can't get it
     }
     
     if (host_statistics(host_port, HOST_VM_INFO, (host_info_t)&vm_stats, &count_info) != KERN_SUCCESS) {
         free(processes);
         return;
     }
     
     // Total physical memory (approximate)
     int64_t total_mem = (int64_t)page_size * 
                       (vm_stats.free_count + vm_stats.active_count + 
                        vm_stats.inactive_count + vm_stats.wire_count);
     
     for (int i = 0; i < count; i++) {
         pid_t pid = processes[i].kp_proc.p_pid;
         
         if (pid == 0) continue;  // Skip kernel_task
         
         // Get process info
         task_t task;
         if (task_for_pid(mach_task_self(), pid, &task) == KERN_SUCCESS) {
             struct task_basic_info task_info;
             mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
             
             if (task_info(task, TASK_BASIC_INFO, (task_info_t)&task_info, &count) == KERN_SUCCESS) {
                 // Calculate memory usage
                 double mem_usage = (double)task_info.resident_size / (double)total_mem * 100.0;
                 
                 // Get process name
                 char name[256];
                 proc_name(pid, name, sizeof(name));
                 
                 // TODO: Calculate CPU usage (requires multiple sampling)
                 double cpu_usage = 0.0;  // Placeholder
                 
                 add_process(list, pid, name, cpu_usage, mem_usage);
             }
             
             mach_port_deallocate(mach_task_self(), task);
         }
     }
     
     free(processes);
 }
 #else
 /* Get Linux processes */
 static void get_linux_processes(ProcessList *list) {
     DIR *dir;
     struct dirent *entry;
     char path[256];
     char line[256];
     
     // Open /proc directory
     dir = opendir("/proc");
     if (dir == NULL) {
         perror("Cannot open /proc");
         return;
     }
     
     // Get system memory info
     FILE *meminfo = fopen("/proc/meminfo", "r");
     unsigned long total_mem = 0;
     
     if (meminfo) {
         if (fgets(line, sizeof(line), meminfo)) {
             sscanf(line, "MemTotal: %lu kB", &total_mem);
             total_mem *= 1024;  // Convert to bytes
         }
         fclose(meminfo);
     }
     
     // Read through each directory in /proc
     while ((entry = readdir(dir)) != NULL) {
         // Check if the name is a number (pid)
         if (entry->d_name[0] >= '0' && entry->d_name[0] <= '9') {
             int pid = atoi(entry->d_name);
             
             // Get process name from /proc/[pid]/comm
             snprintf(path, sizeof(path), "/proc/%d/comm", pid);
             FILE *comm = fopen(path, "r");
             char name[256] = "<unknown>";
             
             if (comm) {
                 if (fgets(name, sizeof(name), comm)) {
                     // Remove newline
                     size_t len = strlen(name);
                     if (len > 0 && name[len-1] == '\n') {
                         name[len-1] = '\0';
                     }
                 }
                 fclose(comm);
             }
             
             // Get memory usage
             snprintf(path, sizeof(path), "/proc/%d/statm", pid);
             FILE *statm = fopen(path, "r");
             unsigned long rss = 0;
             
             if (statm) {
                 if (fscanf(statm, "%*u %lu", &rss) == 1) {
                     rss *= sysconf(_SC_PAGESIZE);  // Convert to bytes
                 }
                 fclose(statm);
             }
             
             // Calculate memory usage percentage
             double mem_usage = 0.0;
             if (total_mem > 0) {
                 mem_usage = (double)rss / (double)total_mem * 100.0;
             }
             
             // Get CPU usage (simplified, more accurate would require multiple samples)
             double cpu_usage = 0.0;
             snprintf(path, sizeof(path), "/proc/%d/stat", pid);
             FILE *stat = fopen(path, "r");
             
             if (stat) {
                 unsigned long utime, stime;
                 if (fscanf(stat, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %lu %lu", 
                           &utime, &stime) == 2) {
                     // This is a simplified version. For accurate CPU usage,
                     // we would need to sample at intervals and calculate deltas
                     cpu_usage = (utime + stime) / 100.0;  // Just a placeholder
                 }
                 fclose(stat);
             }
             
             add_process(list, pid, name, cpu_usage, mem_usage);
         }
     }
     
     closedir(dir);
 }
 #endif