#include <stdio.h>
#include <stdlib.h>
#include "datac.h"

static void print_students(const DatacTable *t) {
    for (int i = 0; i < t->row_n; i++) {
        DatacRow *r = &t->rows[i];
        int day, month, year;
        datac_date(r, "birthday", &day, &month, &year);
        printf("  %-8s  age=%-2ld  grade=%-3ld  enrolled=%-5s  bday=%02d.%02d.%04d%s\n",
               datac_str(r, "name"),
               datac_int(r, "age"),
               datac_int(r, "grade"),
               datac_bool(r, "enrolled") ? "true" : "false",
               day, month, year,
               datac_null(r, "gpa") ? "  gpa=NIL"
                                    : "");
    }
}

int main(void) {
    /* ── Open all tables ────────────────────────────────────────────────────── */
    DatacFile *file = datac_open("data/all.datac");
    if (!file) { fprintf(stderr, "open failed\n"); return 1; }

    printf("=== %d tables loaded ===\n\n", file->table_n);

    DatacTable *students = datac_table(file, "students");
    DatacTable *scores   = datac_table(file, "scores");

    /* ── Schema introspection ───────────────────────────────────────────────── */
    printf("Students schema:\n");
    for (int i = 0; i < students->schema_n; i++) {
        const DatacField *f = &students->schema[i];
        printf("  %-14s  %-8s%s%s\n",
               f->name, datac_type_name(f->type),
               f->is_primary  ? "  [primary]"  : "",
               f->is_nullable ? "  [nullable]" : "");
    }
    printf("\n");

    /* ── Read all students ──────────────────────────────────────────────────── */
    printf("All students:\n");
    print_students(students);
    printf("\n");

    /* ── datac_insert ───────────────────────────────────────────────────────── */
    const char *keys[] = { "name",  "age", "grade", "house", "enrolled", "birthday" };
    const char *vals[] = { "Blake", "14",  "78",    "Green", "true",     "05.09.11" };
    datac_insert(students, keys, vals, 6);
    printf("After insert (Blake):\n");
    print_students(students);
    printf("\n");

    /* ── datac_update_str ───────────────────────────────────────────────────── */
    int changed = datac_update_str(students, "name", "Blake", "house", "Red");
    printf("Updated Blake's house → Red (%d row changed)\n", changed);

    /* ── datac_update_int ───────────────────────────────────────────────────── */
    changed = datac_update_int(students, "age", 16, "grade", 95);
    printf("Set grade=95 for all age-16 students (%d rows changed)\n\n", changed);
    print_students(students);
    printf("\n");

    /* ── datac_update_bool ──────────────────────────────────────────────────── */
    changed = datac_update_bool(students, "enrolled", 0, "enrolled", 1);
    printf("Re-enrolled all unenrolled students (%d changed)\n\n", changed);

    /* ── datac_set_nil_str ──────────────────────────────────────────────────── */
    changed = datac_set_nil_str(students, "name", "Blake", "house");
    printf("Set Blake.house to nil (%d changed); null check: %d\n\n",
           changed, datac_null(&students->rows[students->row_n - 1], "house"));

    /* ── datac_find_null ────────────────────────────────────────────────────── */
    DatacRow *nil_rows[16];
    int nil_n = datac_find_null(students, "gpa", nil_rows, 16);
    printf("Students with nil gpa: %d\n", nil_n);
    for (int i = 0; i < nil_n; i++)
        printf("  %s\n", datac_str(nil_rows[i], "name"));
    printf("\n");

    /* ── datac_delete_str ───────────────────────────────────────────────────── */
    int removed = datac_delete_str(students, "name", "Blake");
    printf("Deleted Blake (%d row removed); students now: %d rows\n\n",
           removed, students->row_n);

    /* ── datac_delete_null ──────────────────────────────────────────────────── */
    removed = datac_delete_null(students, "gpa");
    printf("Deleted rows with nil gpa (%d removed); students now: %d rows\n\n",
           removed, students->row_n);

    /* ── datac_delete_bool ──────────────────────────────────────────────────── */
    removed = datac_delete_bool(students, "enrolled", 0);
    printf("Deleted unenrolled students (%d removed)\n\n", removed);

    /* ── datac_update_int_add / sub / mul ───────────────────────────────────── */
    changed = datac_update_int_add(students, "age", 15, "grade", 5);
    printf("grade += 5 for age-15 students (%d changed)\n", changed);

    changed = datac_update_int_sub(students, "age", 16, "grade", 2);
    printf("grade -= 2 for age-16 students (%d changed)\n", changed);

    changed = datac_update_int_mul(students, "age", 14, "grade", 2);
    printf("grade *= 2 for age-14 students (%d changed)\n\n", changed);

    /* ── datac_set_nil_int ──────────────────────────────────────────────────── */
    changed = datac_set_nil_int(students, "grade", 0, "grade");
    printf("Zeroed grades set to nil (%d changed)\n\n", changed);

    /* ── Aggregates ─────────────────────────────────────────────────────────── */
    printf("Aggregates on remaining students (%d rows):\n", students->row_n);
    printf("  count(age)   = %d\n",   datac_count    (students, "age"));
    printf("  sum(grade)   = %ld\n",  datac_sum_int  (students, "grade"));
    printf("  mean(grade)  = %.2f\n", datac_mean     (students, "grade"));
    printf("  min(age)     = %ld\n",  datac_min_int  (students, "age"));
    printf("  max(age)     = %ld\n",  datac_max_int  (students, "age"));
    printf("  min(name)    = %s\n",   datac_min_str  (students, "name") ?: "(none)");
    printf("  max(name)    = %s\n",   datac_max_str  (students, "name") ?: "(none)");
    printf("\n");

    /* ── datac_sort ─────────────────────────────────────────────────────────── */
    printf("Students sorted by age ascending:\n");
    datac_sort(students, "age", 0);
    print_students(students);
    printf("\n");

    printf("Students sorted by grade descending:\n");
    datac_sort(students, "grade", 1);
    print_students(students);
    printf("\n");

    /* ── scores: deltat + aggregates ────────────────────────────────────────── */
    printf("Scores (%d rows):\n", scores->row_n);
    for (int i = 0; i < scores->row_n; i++) {
        DatacRow *r = &scores->rows[i];
        long secs   = datac_deltat_seconds(r, "session_time");
        printf("  %-8s  score=%-4ld  %ld sec\n",
               datac_str(r, "player"),
               datac_int(r, "score"),
               secs);
    }
    printf("  sum(score)  = %ld\n",  datac_sum_int  (scores, "score"));
    printf("  mean(score) = %.2f\n", datac_mean     (scores, "score"));
    printf("  min(score)  = %ld\n",  datac_min_int  (scores, "score"));
    printf("  max(score)  = %ld\n",  datac_max_int  (scores, "score"));
    printf("\n");

    datac_sort(scores, "session_time", 1);
    printf("Scores sorted by session_time desc:\n");
    for (int i = 0; i < scores->row_n; i++) {
        DatacRow *r = &scores->rows[i];
        printf("  %-8s  %ld sec\n",
               datac_str(r, "player"),
               datac_deltat_seconds(r, "session_time"));
    }
    printf("\n");

    /* ── datac_update_float ─────────────────────────────────────────────────── */
    changed = datac_update_float_add(scores, "score", (double)datac_max_int(scores, "score"),
                                     "score", 10.0);
    printf("Added 10 to top score (%d changed)\n\n", changed);

    /* ── Programmatic table construction ────────────────────────────────────── */
    printf("Building 'medals' table programmatically:\n");
    DatacTable *medals = datac_create("medals");
    datac_add_field_def(medals, "player",  DATAC_STRING, 1, 0);
    datac_add_field_def(medals, "rank",    DATAC_INT,    0, 0);
    datac_add_field_def(medals, "note",    DATAC_STRING, 0, 1);  /* nullable */

    const char *mk1[] = { "player", "rank" };
    const char *mv1[] = { "Alice",  "1"    };
    datac_insert(medals, mk1, mv1, 2);

    const char *mk2[] = { "player", "rank", "note" };
    const char *mv2[] = { "Bob",    "2",    "rising star" };
    datac_insert(medals, mk2, mv2, 3);

    for (int i = 0; i < medals->row_n; i++) {
        DatacRow *r = &medals->rows[i];
        printf("  %-8s  rank=%ld  note=%s\n",
               datac_str(r, "player"),
               datac_int(r, "rank"),
               datac_null(r, "note") ? "NIL" : datac_str(r, "note"));
    }
    printf("\n");

    /* ── datac_file_create + datac_file_add_table ───────────────────────────── */
    DatacFile *out_file = datac_file_create();
    datac_file_add_table(out_file, medals);
    free(medals);   /* file owns the data now; free the shell only */

    if (datac_save(out_file, "/tmp/medals.datac"))
        printf("Saved medals → /tmp/medals.datac\n");

    /* Re-read it to prove the round-trip works */
    DatacTable *medals_rt = datac_load("/tmp/medals.datac");
    if (medals_rt) {
        printf("Round-trip: %d rows, schema_n=%d\n",
               medals_rt->row_n, medals_rt->schema_n);
        datac_free(medals_rt);
    }
    datac_close(out_file);

    /* ── datac_save (whole original file) ───────────────────────────────────── */
    if (datac_save(file, "/tmp/c_out.datac"))
        printf("Saved all tables → /tmp/c_out.datac\n");

    /* ── datac_save_table (single table) ────────────────────────────────────── */
    if (datac_save_table(students, "/tmp/students_only.datac"))
        printf("Saved students → /tmp/students_only.datac\n");

    datac_close(file);
    return 0;
}
