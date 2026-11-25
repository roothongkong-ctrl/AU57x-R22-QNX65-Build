/* Synthetic QNX process: tests QNX 6.5 LD_PRELOAD and RTLD_NEXT only.
 * This is not the OEM btstack and cannot prove Bluetooth/iAP2 ownership.
 */
#define _QNX_SOURCE 1
#include <pthread.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define LOG_PATH "/dev/shmem/au57x_r142_thread_probe.log"
static void *worker(void *unused) {
    (void)unused;
    int a = pthread_setname_np(pthread_self(), "Thread_bluetoothProc");
    int b = pthread_setname_np(pthread_self(), "bluesdk_mv8787bt_read");
    int c = pthread_setname_np(pthread_self(), "bluesdk_mv8787bt_write");
    printf("R142_QNX_FORWARD_RC primary=%d read=%d write=%d\n", a, b, c);
    return (void *)(long)((a != 0) || (b != 0) || (c != 0));
}
int main(void) {
    pthread_t thread; void *thread_result = 0;
    if (pthread_create(&thread, 0, worker, 0) != 0) { puts("R142_QNX_FAIL pthread_create"); return 10; }
    if (pthread_join(thread, &thread_result) != 0 || (long)thread_result != 0) { puts("R142_QNX_FAIL pthread_join_or_forward"); return 11; }
    int fd = open(LOG_PATH, O_RDONLY);
    if (fd < 0) { puts("R142_QNX_FAIL observer_log_missing_or_constructor_inactive"); return 12; }
    char buf[2048]; ssize_t n = read(fd, buf, sizeof(buf) - 1); close(fd);
    if (n <= 0) { puts("R142_QNX_FAIL observer_log_empty"); return 13; }
    buf[n] = '\0';
    if (!strstr(buf, "event=observer_ready") ||
        !strstr(buf, "role=primary_bluetooth_proc_candidate") ||
        !strstr(buf, "name=Thread_bluetoothProc") ||
        !strstr(buf, "role=secondary_bluesdk_read_worker") ||
        !strstr(buf, "name=bluesdk_mv8787bt_read") ||
        !strstr(buf, "role=secondary_bluesdk_write_worker") ||
        !strstr(buf, "name=bluesdk_mv8787bt_write")) {
        puts("R142_QNX_FAIL observer_classification_mismatch"); return 14;
    }
    puts("R142_QNX_LD_PRELOAD_CONSTRUCTOR_PASS");
    puts("R142_QNX_RTLD_NEXT_FORWARD_PASS");
    puts("R142_QNX_THREAD_NAME_CLASSIFICATION_PASS");
    puts("R142_QNX_SYNTHETIC_PROCESS_ONLY_NOT_OEM_BTSTACK");
    return 0;
}
