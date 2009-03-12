#include "syslog-ng.h"
#include "syslog-names.h"
#include "filter.h"
#include "logmsg.h"
#include "apphook.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int debug = 1;
GSockAddr *sender_saddr;

static gint
facility_bits(gchar *fac)
{
  return 1 << (syslog_name_lookup_facility_by_name(fac) >> 3);
}

static gint
level_bits(gchar *lev)
{
  return 1 << syslog_name_lookup_level_by_name(lev);
}

static gint
level_range(gchar *from, gchar *to)
{
  int r1, r2;
  
  r1 = syslog_name_lookup_level_by_name(from); 
  r2 = syslog_name_lookup_level_by_name(to);
  return syslog_make_range(r1, r2);
}

FilterExprNode *
create_posix_regexp_filter(gint field, gchar* regexp, gint flags)
{
  FilterRE *f;
  f = (FilterRE*)filter_re_new((gchar *) GINT_TO_POINTER(field));
  filter_re_set_matcher(f, log_matcher_posix_re_new());
  filter_re_set_flags(f, flags);
  if (filter_re_set_regexp(f, regexp))
    return &f->super;
  else 
    return NULL;
}

FilterExprNode *
create_posix_regexp_match(gchar* regexp, gint flags)
{
  FilterRE *f;
  f = (FilterRE*)filter_match_new();
  filter_re_set_matcher(f, log_matcher_posix_re_new());
  filter_re_set_flags(f, flags);
  if (filter_re_set_regexp(f, regexp))
    return &f->super;
  else 
    return NULL;
}

#if ENABLE_PCRE
FilterExprNode *
create_pcre_regexp_filter(gint field, gchar* regexp, gint flags)
{
  FilterRE *f;
  f = (FilterRE*)filter_re_new((gchar *) GINT_TO_POINTER(field));
  filter_re_set_matcher(f, log_matcher_pcre_re_new());
  filter_re_set_flags(f, flags);
  if (filter_re_set_regexp(f, regexp))
    return &f->super;
  else 
    return NULL;
}

FilterExprNode *
create_pcre_regexp_match(gchar* regexp, gint flags)
{
  FilterRE *f;
  f = (FilterRE*)filter_match_new();
  filter_re_set_matcher(f, log_matcher_pcre_re_new());
  filter_re_set_flags(f, flags);
  if (filter_re_set_regexp(f, regexp))
    return &f->super;
  else 
    return NULL;
}
#endif

void
testcase(gchar *msg, 
         gint parse_flags,
         FilterExprNode *f,
         gboolean expected_result)
{
  LogMessage *logmsg;
  gboolean res;
  static gint testno = 0;
  
  testno++;
  logmsg = log_msg_new(msg, strlen(msg), NULL, parse_flags, NULL, -1, 0xFFFF);
  logmsg->saddr = g_sockaddr_ref(sender_saddr);
  
  res = filter_expr_eval(f, logmsg);
  if (res != expected_result)
    {
      fprintf(stderr, "Filter test failed; num='%d', msg='%s'\n", testno, msg);
      exit(1);
    }
    
  f->comp = 1;
  res = filter_expr_eval(f, logmsg);
  if (res != !expected_result)
    {
      fprintf(stderr, "Filter test failed (negated); num='%d', msg='%s'\n", testno, msg);
      exit(1);
    }
  
  log_msg_unref(logmsg);
  filter_expr_free(f);
}

void
testcase_with_backref_chk(gchar *msg, 
         gint parse_flags,
         FilterExprNode *f,
         gboolean expected_result, 
         const gchar *name,
         const gchar *value
         )
{
  LogMessage *logmsg;
  gchar *value_msg;
  gboolean res;
  static gint testno = 0;
  gssize length;
  
  testno++;
  logmsg = log_msg_new(msg, strlen(msg), NULL, parse_flags, NULL, -1, 0xFFFF);
  logmsg->saddr = g_sockaddr_inet_new("10.10.0.1", 5000);
  
  /* NOTE: we test how our filters cope with non-zero terminated values. We don't change message_len, only the value */
  LOG_MESSAGE_WRITABLE_FIELD(logmsg->message) = g_realloc(logmsg->message, logmsg->message_len + 10);
  memset(logmsg->message + logmsg->message_len, 'A', 10);
  
  res = filter_expr_eval(f, logmsg);
  if (res != expected_result)
    {
      fprintf(stderr, "Filter test failed; num='%d', msg='%s'\n", testno, msg);
      exit(1);
    }
    
  f->comp = 1;
  res = filter_expr_eval(f, logmsg);
  if (res != !expected_result)
    {
      fprintf(stderr, "Filter test failed (negated); num='%d', msg='%s'\n", testno, msg);
      exit(1);
    }
  value_msg = log_msg_get_value(logmsg, name, &length);

  if(value == NULL || value[0] == 0) 
     {  
       if (value_msg != NULL && value_msg[0] != 0)
         {
           fprintf(stderr, "Filter test failed (NULL value chk); num='%d', msg='%s', expected_value='%s', value_in_msg='%s'", testno, msg, value, value_msg);
           exit(1);
         }
      }
  else 
    { 
      if (strncmp(value_msg, value, length) != 0)
        {
          fprintf(stderr, "Filter test failed (value chk); num='%d', msg='%s', expected_value='%s', value_in_msg='%s'", testno, msg, value, value_msg);
          exit(1);
        }
    }
  log_msg_unref(logmsg);
  filter_expr_free(f);
}


#define TEST_ASSERT(cond)                                       \
  if (!(cond))                                                  \
    {                                                           \
      fprintf(stderr, "Test assertion failed at %d\n", __LINE__);    \
      exit(1);                                                  \
    }

int 
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  gint i;
  
  app_startup();
  
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("user")), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("daemon")), 0);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("daemon") | facility_bits("user")), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("uucp") | facility_bits("local4")), 0);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(0x80000000 | (LOG_USER >> 3)), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(0x80000000 | (LOG_DAEMON >> 3)), 0);
  
  testcase("<2> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("kern")), 1);
  testcase("<2> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(0x80000000 | (LOG_KERN >> 3)), 1);
  
  testcase("<128> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("local0")), 1);
  testcase("<128> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(0x80000000 | (LOG_LOCAL0 >> 3)), 1);
  testcase("<32> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("local1")), 0);
  testcase("<32> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("auth")), 1);
  testcase("<32> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(0x80000000 | (LOG_AUTH >> 3)), 1);
#ifdef LOG_AUTHPRIV
  testcase("<80> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(facility_bits("authpriv")), 1);
  testcase("<80> openvpn[2499]: PTHREAD support initialized", 0, filter_facility_new(0x80000000 | (LOG_AUTHPRIV >> 3)), 1);
#endif

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("debug") | level_bits("emerg")), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("emerg")), 0);

  testcase("<8> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("crit", "emerg")), 1);
  testcase("<9> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("crit", "emerg")), 1);
  testcase("<10> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("crit", "emerg")), 1);
  testcase("<11> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("crit", "emerg")), 0);
  testcase("<12> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("crit", "emerg")), 0);
  testcase("<13> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("crit", "emerg")), 0);
  testcase("<14> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("crit", "emerg")), 0);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("crit", "emerg")), 0);

  testcase("<8> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("debug", "notice")), 0);
  testcase("<9> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("debug", "notice")), 0);
  testcase("<10> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("debug", "notice")), 0);
  testcase("<11> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("debug", "notice")), 0);
  testcase("<12> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("debug", "notice")), 0);
  testcase("<13> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("debug", "notice")), 1);
  testcase("<14> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("debug", "notice")), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_range("debug", "notice")), 1);

  for (i = 0; i < 10; i++)
    {
      testcase("<0> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("emerg")), 1);
      testcase("<1> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("alert")), 1);
      testcase("<2> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("crit")), 1);
      testcase("<3> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("err")), 1);
      testcase("<4> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("warning")), 1);
      testcase("<5> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("notice")), 1);
      testcase("<6> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("info")), 1);
      testcase("<7> openvpn[2499]: PTHREAD support initialized", 0, filter_level_new(level_bits("debug")), 1);
    }
    
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, create_posix_regexp_filter(LM_F_PROGRAM, "^openvpn$", 0), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, create_posix_regexp_filter(LM_F_PROGRAM, "^open$", 0), 0);
  fprintf(stderr, "One \"invalid regular expression\" message is to be expected\n");
  TEST_ASSERT(create_posix_regexp_filter(LM_F_PROGRAM, "((", 0) == NULL);
  
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_posix_regexp_filter(LM_F_HOST, "^host$", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_posix_regexp_filter(LM_F_HOST, "^hos$", 0), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_posix_regexp_filter(LM_F_HOST, "pthread", 0), 0);
  fprintf(stderr, "One \"invalid regular expressions\" message is to be expected\n");
  TEST_ASSERT(create_posix_regexp_filter(LM_F_HOST, "((", 0) == NULL);
  

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_posix_regexp_filter(LM_F_MESSAGE, "^PTHREAD ", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_posix_regexp_filter(LM_F_MESSAGE, "PTHREAD s", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_posix_regexp_filter(LM_F_MESSAGE, "^PTHREAD$", 0), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_posix_regexp_filter(LM_F_MESSAGE, "(?i)pthread", 0), 1);
  

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_posix_regexp_match(" PTHREAD ", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_posix_regexp_match("^openvpn\\[2499\\]: PTHREAD", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_posix_regexp_match("^PTHREAD$", 0), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_posix_regexp_match("(?i)pthread", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_posix_regexp_match("pthread", LMF_ICASE), 1);
  

  fprintf(stderr, "One \"invalid regular expression\" message is to be expected\n");
  TEST_ASSERT(create_posix_regexp_match("((", 0) == NULL);
  

  sender_saddr = g_sockaddr_inet_new("10.10.0.1", 5000);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_netmask_new("10.10.0.0/16"), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_netmask_new("10.10.0.0/24"), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_netmask_new("10.10.10.0/24"), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_netmask_new("0.0.10.10/24"), 0);
  g_sockaddr_unref(sender_saddr);
  sender_saddr = NULL;
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_netmask_new("127.0.0.1/32"), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, filter_netmask_new("127.0.0.2/32"), 0);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_or_new(create_posix_regexp_match(" PTHREAD ", 0), create_posix_regexp_match("PTHREAD", 0)), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_or_new(create_posix_regexp_match(" PTHREAD ", 0), create_posix_regexp_match("^PTHREAD$", 0)), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_or_new(create_posix_regexp_match("^PTHREAD$", 0), create_posix_regexp_match(" PTHREAD ", 0)), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_or_new(create_posix_regexp_match(" PAD ", 0), create_posix_regexp_match("^PTHREAD$", 0)), 0);
  
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_and_new(create_posix_regexp_match(" PTHREAD ", 0), create_posix_regexp_match("PTHREAD", 0)), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_and_new(create_posix_regexp_match(" PTHREAD ", 0), create_posix_regexp_match("^PTHREAD$", 0)), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_and_new(create_posix_regexp_match("^PTHREAD$", 0), create_posix_regexp_match(" PTHREAD ", 0)), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_and_new(create_posix_regexp_match(" PAD ", 0), create_posix_regexp_match("^PTHREAD$", 0)), 0);
  


  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", 0, create_posix_regexp_filter(LM_F_MESSAGE, "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "1","a");
  
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", 0, create_posix_regexp_filter(LM_F_MESSAGE, "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "0","al fa");
  
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", 0, create_posix_regexp_filter(LM_F_MESSAGE, "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "232", NULL);
  

#if ENABLE_PCRE
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, create_pcre_regexp_filter(LM_F_PROGRAM, "^openvpn$", 0), 1);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, create_pcre_regexp_filter(LM_F_PROGRAM, "^open$", 0), 0);
  fprintf(stderr, "One \"invalid regular expression\" message is to be expected\n");
  TEST_ASSERT(create_pcre_regexp_filter(LM_F_PROGRAM, "((", 0) == NULL);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_pcre_regexp_filter(LM_F_HOST, "^host$", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_pcre_regexp_filter(LM_F_HOST, "^hos$", 0), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_pcre_regexp_filter(LM_F_HOST, "pthread", 0), 0);
  fprintf(stderr, "One \"invalid regular expressions\" message is to be expected\n");
  TEST_ASSERT(create_pcre_regexp_filter(LM_F_HOST, "((", 0) == NULL);

  fprintf(stderr, "One \"invalid regular expressions\" message is to be expected\n");
  TEST_ASSERT(create_posix_regexp_filter(LM_F_HOST, "(?iana", 0) == NULL);
  
  fprintf(stderr, "One \"invalid regular expressions\" message is to be expected\n");
  TEST_ASSERT(create_pcre_regexp_filter(LM_F_HOST, "(?iana", 0) == NULL);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_pcre_regexp_filter(LM_F_MESSAGE, "^PTHREAD ", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_pcre_regexp_filter(LM_F_MESSAGE, "PTHREAD s", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_pcre_regexp_filter(LM_F_MESSAGE, "^PTHREAD$", 0), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_pcre_regexp_filter(LM_F_MESSAGE, "(?i)pthread", 0), 1);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_and_new(create_pcre_regexp_match(" PTHREAD ", 0), create_posix_regexp_match("PTHREAD", 0)), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_and_new(create_pcre_regexp_match(" PTHREAD ", 0), create_posix_regexp_match("^PTHREAD$", 0)), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_and_new(create_pcre_regexp_match("^PTHREAD$", 0), create_posix_regexp_match(" PTHREAD ", 0)), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_and_new(create_pcre_regexp_match(" PAD ", 0), create_posix_regexp_match("^PTHREAD$", 0)), 0);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_or_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("PTHREAD", 0)), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_or_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_or_new(create_pcre_regexp_match("^PTHREAD$", 0), create_pcre_regexp_match(" PTHREAD ", 0)), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, fop_or_new(create_pcre_regexp_match(" PAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), 0);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_pcre_regexp_match(" PTHREAD ", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_pcre_regexp_match("^openvpn\\[2499\\]: PTHREAD", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_pcre_regexp_match("^PTHREAD$", 0), 0);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_pcre_regexp_match("(?i)pthread", 0), 1);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", 0, create_pcre_regexp_match("pthread", LMF_ICASE), 1);

  fprintf(stderr, "One \"invalid regular expression\" message is to be expected\n");
  TEST_ASSERT(create_pcre_regexp_match("((", 0) == NULL);

  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: alma fa", 0, create_pcre_regexp_filter(LM_F_MESSAGE, "(?P<a>a)(?P<l>l)(?P<MM>m)(?P<aa>a) (?P<fa>fa)", LMF_STORE_MATCHES), 1, "MM","m");
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: alma fa", 0, create_pcre_regexp_filter(LM_F_MESSAGE, "(?P<a>a)(?P<l>l)(?P<MM>m)(?P<aa>a) (?P<fa>fa)", LMF_STORE_MATCHES), 1, "aaaa", NULL);
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: alma fa", 0, create_pcre_regexp_filter(LM_F_MESSAGE, "(?P<a>a)(?P<l>l)(?P<MM>m)(?P<aa>a) (?P<fa_name>fa)", LMF_STORE_MATCHES), 1, "fa_name","fa");

  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", 0, create_pcre_regexp_filter(LM_F_MESSAGE, "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "2","l");
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", 0, create_pcre_regexp_filter(LM_F_MESSAGE, "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "0","al fa");
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", 0, create_pcre_regexp_filter(LM_F_MESSAGE, "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "233",NULL);
#endif

  app_shutdown();
  return 0;
}

