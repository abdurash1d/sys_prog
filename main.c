#include <gtk/gtk.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <pthread.h>

#define MAX_PROCESSES 1024

typedef struct {
    int pid;
    char name[256];
    char username[256];
    float cpu_usage;
    float memory_percent;
    unsigned long memory_kb;
} ProcessInfo;

GtkWidget *process_list;
GtkListStore *list_store;
GtkTreeSelection *selection;
GtkWidget *refresh_rate_combo;
ProcessInfo processes[MAX_PROCESSES];
int process_count = 0;
int refresh_interval = 2;
bool is_running = TRUE;
pthread_t update_thread;
pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

void get_username(uid_t uid, char *username) {
    struct passwd *pw = getpwuid(uid);
    if (pw) strcpy(username, pw->pw_name);
    else strcpy(username, "unknown");
}

unsigned long get_total_memory_kb() {
    FILE *fp = fopen("/proc/meminfo", "r");
    char line[256];
    unsigned long mem_total = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %lu kB", &mem_total) == 1) break;
    }
    fclose(fp);
    return mem_total;
}

void fetch_processes_info() {
    DIR *dir = opendir("/proc");
    struct dirent *entry;
    int count = 0;
    unsigned long total_mem = get_total_memory_kb();

    while ((entry = readdir(dir)) != NULL && count < MAX_PROCESSES) {
        if (!isdigit(*entry->d_name)) continue;

        int pid = atoi(entry->d_name);
        char stat_path[64], status_path[64], cmdline_path[64];
        sprintf(stat_path, "/proc/%d/stat", pid);
        sprintf(status_path, "/proc/%d/status", pid);
        sprintf(cmdline_path, "/proc/%d/cmdline", pid);

        FILE *stat_fp = fopen(stat_path, "r");
        FILE *status_fp = fopen(status_path, "r");
        if (!stat_fp || !status_fp) continue;

        ProcessInfo *p = &processes[count];
        p->pid = pid;
        fscanf(stat_fp, "%*d (%[^)])", p->name);

        char line[256];
        uid_t uid = 0;
        unsigned long vm_rss = 0;
        while (fgets(line, sizeof(line), status_fp)) {
            if (sscanf(line, "Uid: %d", &uid) == 1) continue;
            if (sscanf(line, "VmRSS: %lu kB", &vm_rss) == 1) continue;
        }
        fclose(stat_fp);
        fclose(status_fp);

        get_username(uid, p->username);
        p->memory_kb = vm_rss;
        p->memory_percent = total_mem ? ((float)vm_rss / total_mem) * 100.0f : 0.0f;
        p->cpu_usage = 0.0f; // Simplified version without real-time CPU tracking

        count++;
    }

    pthread_mutex_lock(&data_mutex);
    process_count = count;
    pthread_mutex_unlock(&data_mutex);
    closedir(dir);
}

void refresh_data() {
    fetch_processes_info();
    pthread_mutex_lock(&data_mutex);
    gtk_list_store_clear(list_store);
    for (int i = 0; i < process_count; i++) {
        GtkTreeIter iter;
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter,
            0, processes[i].pid,
            1, processes[i].name,
            2, processes[i].username,
            3, processes[i].cpu_usage,
            4, processes[i].memory_percent,
            5, g_strdup_printf("%lu", processes[i].memory_kb),
            -1);
    }
    pthread_mutex_unlock(&data_mutex);
}

void *update_data_thread(void *arg) {
    while (is_running) {
        refresh_data();
        sleep(refresh_interval);
    }
    return NULL;
}

void on_refresh_clicked(GtkButton *button, gpointer user_data) {
    refresh_data();
}

void on_kill_clicked(GtkButton *button, gpointer user_data) {
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gint pid;
        gtk_tree_model_get(model, &iter, 0, &pid, -1);
        if (kill(pid, SIGKILL) == 0) {
            refresh_data();
        } else {
            perror("Kill failed");
        }
    }
}

void on_refresh_rate_changed(GtkComboBox *combo, gpointer user_data) {
    const gchar *text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
    if (text) refresh_interval = atoi(text);
}

void create_columns(GtkWidget *treeview) {
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    const char *titles[] = {"PID", "Name", "User", "CPU %", "MEM %", "Memory KB"};

    for (int i = 0; i < 6; i++) {
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(titles[i], renderer, "text", i, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    }
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "System Process Tracker");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    list_store = gtk_list_store_new(6, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_FLOAT, G_TYPE_FLOAT, G_TYPE_STRING);
    process_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(process_list));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    create_columns(process_list);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), process_list);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 5);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *refresh_button = gtk_button_new_with_label("Refresh");
    GtkWidget *kill_button = gtk_button_new_with_label("Kill Process");
    GtkWidget *refresh_rate_label = gtk_label_new("Refresh rate (s):");
    refresh_rate_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(refresh_rate_combo), "1");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(refresh_rate_combo), "2");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(refresh_rate_combo), "5");
    gtk_combo_box_set_active(GTK_COMBO_BOX(refresh_rate_combo), 1);

    g_signal_connect(refresh_button, "clicked", G_CALLBACK(on_refresh_clicked), NULL);
    g_signal_connect(kill_button, "clicked", G_CALLBACK(on_kill_clicked), NULL);
    g_signal_connect(refresh_rate_combo, "changed", G_CALLBACK(on_refresh_rate_changed), NULL);

    gtk_box_pack_start(GTK_BOX(hbox), refresh_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), kill_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), refresh_rate_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), refresh_rate_combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

    refresh_data();
    pthread_create(&update_thread, NULL, update_data_thread, NULL);

    gtk_widget_show_all(window);
    gtk_main();

    is_running = FALSE;
    pthread_join(update_thread, NULL);
    pthread_mutex_destroy(&data_mutex);
    return 0;
}
