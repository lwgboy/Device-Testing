#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <pcap.h>
#include <netinet/in.h>
#include <sys/select.h>
#include "thc-ipv6.h"

char *interface = NULL, *dns_name = NULL, elapsed[6] = { 0, 8, 0, 2, 0, 0 };
int counter = 0;

// start0: 1-3 rand, 18-21 rand, 22-27 mac, 32-35 rand
char solicit[] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x02, 0x00, 0x00,
  0x00, 0x01, 0x00, 0x0e, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};
char dnsupdate1[] = { 0, 39, 0, 8, 1, 6, 122, 97, 97, 97, 97, 97 };
char dnsupdate2[] = { 0, 6, 0, 2, 0, 39 };

void help(char *prg) {
  printf("%s %s (c) 2013 by %s %s\n\n", prg, VERSION, AUTHOR, RESOURCE);
  printf("Syntax: %s interface\n\n", prg);
  printf("DHCPv6 information tool. Dumps the available servers and their setup.\n");
  exit(-1);
}

void clean_exit(int signo) {
  printf("\n%d server%s found\n", counter, counter == 1 ? "" : "s");
  exit(0);
}

void check_packets(u_char *foo, const struct pcap_pkthdr *header, const unsigned char *data) {
  int len = header->caplen, rlen, i, j;
  unsigned char *ptr = (unsigned char *) data, *rdata;
  char mybuf[1024] = { 0x03, 0, 0, 0, 0, 8, 0, 2, 0, 0 };

  if (do_hdr_size) {
    data += do_hdr_size;
    len -= do_hdr_size;
    if ((data[0] & 240) != 0x60)
      return;
  } else {
    data += 14;
    len -= 14;
  }
  rlen = len;
  rdata = (unsigned char *) data;

  if (len < 126 || data[6] != NXT_UDP || data[48] != 2)
    return;

  data += 48;
  len -= 48;

  memcpy(mybuf + 1, data + 1, 3);
  data += 4;
  len -= 4;

/*
  while (len >= 4) {
    if ((olen = data[2] * 256 + data[3]) > len - 4 || olen < 0) {
      printf("Information: evil packet received\n");
      olen = 0;
      len = -1;
    } else {
      if (data[1] > 1 && data[1] <= 3) {
        memcpy(mybuf + mlen, data, olen + 4);
        mlen += olen + 4;
      } else if (data[1] == 1) {
        memcpy(mybuf + mlen, data, olen + 4);
        mlen += olen + 4;
        if (olen == 14)
          smac = (char *) (data + 12);
        else
          smac = mac;
      }
      data += olen + 4;
      len -= olen + 4;
      if (len < 0) {
        printf("Information: evil packet received\n");
        len = -1;
      }
    }
  }
*/
  if (len >= 4) {
    counter++;
    printf("\nDHCPv6 packet received:\n");
    printf("  Server IP6: %s\n", thc_ipv62notation(rdata + 8));
    printf("  Server MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", ptr[6], ptr[7], ptr[8], ptr[9], ptr[10], ptr[11]);
    while (len >= 4) {
      i = data[0] * 256 + data[1];
      j = data[2] * 256 + data[3];
      if (j + 4 > len) {
        printf("Evil Packet!\n");
        return;
      }
      switch(i) {
        case 1:
          printf(""); // client identifier
          break;
        case 2:
          printf(""); // server identier
          break;
        case 3:
          if (data[16] == 0 && data[17] == 5)
            printf("    Address Offered: %s\n", thc_ipv62notation((unsigned char*)data + 20));
          break;
        case 7:
          printf(""); // prefered value
          break;
        case 13:
        case 19:
          printf("    Status Code: %d", data[5] * 256 + data[6]); // status code
          break;
        case 23:
          printf("    DNS Server: %s\n", thc_ipv62notation((unsigned char*)data + 4));
          break;
        default:
          printf("    Unknown option type: %d\n", i);
      }
      len -= (4 + j);
      data += (4 + j);
    }
  }
}

int main(int argc, char *argv[]) {
  char mac[6] = { 0, 0x0c, 0, 0, 0, 0 }, *pkt = NULL;
  char wdatabuf[1024];
  unsigned char *mac6 = mac, *src, *dst;
  int i, s, len, pkt_len = 0;
  unsigned long long int count = 0;
  pcap_t *p = NULL;
  int do_all = 1, use_real_mac = 1, use_real_link = 1;

  if (argc < 2 || strncmp(argv[1], "-h", 2) == 0)
    help(argv[0]);

  if (getenv("THC_IPV6_PPPOE") != NULL || getenv("THC_IPV6_6IN4") != NULL) printf("WARNING: %s is not working with injection!\n", argv[0]);

  while ((i = getopt(argc, argv, "dnNr1")) >= 0) {
    switch (i) {
    case 'N':
      use_real_link = 1;        // no break
    case 'n':
      use_real_mac = 1;
      break;
    case '1':
      do_all = 0;
    case 'r':
      i = 0;
      break;                    // just to ignore -r
    default:
      fprintf(stderr, "Error: unknown option -%c\n", i);
      exit(-1);
    }
  }

  memset(mac, 0, sizeof(mac));
  interface = argv[optind];
  if (thc_get_own_ipv6(interface, NULL, PREFER_LINK) == NULL) {
    fprintf(stderr, "Error: invalid interface %s\n", interface);
    exit(-1);
  }
  dns_name = argv[optind + 1];
  if (use_real_link)
    src = thc_get_own_ipv6(interface, NULL, PREFER_LINK);
  else
    src = thc_resolve6("fe80::");
  if (use_real_mac)
    mac6 = thc_get_own_mac(interface);
  dst = thc_resolve6("ff02::1:2");
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  // only to prevent our system to send icmp port unreachable messages
  if ((s = thc_bind_udp_port(546)) < 0)
    fprintf(stderr, "Warning: could not bind to 546/udp\n");
  if ((p = thc_pcap_init_promisc(interface, "ip6 and udp and dst port 546")) == NULL) {
    fprintf(stderr, "Error: can not open interface %s in promisc mode\n", interface);
    exit(-1);
  }
  len = sizeof(solicit);
  memcpy(wdatabuf, solicit, len);

  printf("Sending DHCPv6 Solicitate message ...\n");
  if (!use_real_link)
    memcpy(src + 8, (char *) &count, 8);
    // start0: 1-3 rand, 18-21 rand, 22-27 mac, 32-35 rand
  for (i = 0; i < 3; i++) {
    wdatabuf[i + 32] = rand() % 256;
    wdatabuf[i + 18] = rand() % 256;
    mac[i + 2] = rand() % 256;
  }
  if (!use_real_mac)
    memcpy(wdatabuf + 22, mac, 6);
  memcpy(wdatabuf + 1, (char *) &count + _TAKE3, 3);

  if ((pkt = thc_create_ipv6_extended(interface, PREFER_LINK, &pkt_len, src, dst, 1, 0, 0, 0, 0)) == NULL)
    return -1;
  if (thc_add_udp(pkt, &pkt_len, 546, 547, 0, wdatabuf, len) < 0)
    return -1;
    // we have to tone it down, otherwise we will not get advertisements
  if (thc_generate_and_send_pkt(interface, mac6, NULL, pkt, &pkt_len) < 0)
    printf("!");
  alarm(5);
  signal(SIGALRM, clean_exit);
//  i = thc_send_pkt(interface, pkt, &pkt_len);
  pkt = thc_destroy_packet(pkt);
  while (1) {
    usleep(75);
    while (thc_pcap_check(p, (char *) check_packets, NULL) > 0);
  }

  return 0;                     // never reached
}
