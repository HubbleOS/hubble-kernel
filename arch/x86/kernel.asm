
./build/kernel.elf:     file format elf64-x86-64


Disassembly of section .text:

0000000000100000 <kernel_entry>:
  100000:	48 bc e0 57 10 00 00 	movabs $0x1057e0,%rsp
  100007:	00 00 00 
  10000a:	e8 11 00 00 00       	call   100020 <kernel_main>

000000000010000f <kernel_entry.loop>:
  10000f:	f4                   	hlt
  100010:	eb fd                	jmp    10000f <kernel_entry.loop>
  100012:	66 2e 0f 1f 84 00 00 	cs nopw 0x0(%rax,%rax,1)
  100019:	00 00 00 
  10001c:	0f 1f 40 00          	nopl   0x0(%rax)

0000000000100020 <kernel_main>:
  100020:	55                   	push   %rbp
  100021:	48 89 e5             	mov    %rsp,%rbp
  100024:	53                   	push   %rbx
  100025:	48 89 fb             	mov    %rdi,%rbx
  100028:	48 83 ec 08          	sub    $0x8,%rsp
  10002c:	48 8b 77 20          	mov    0x20(%rdi),%rsi
  100030:	48 8b 7f 18          	mov    0x18(%rdi),%rdi
  100034:	e8 47 0d 00 00       	call   100d80 <heap_init>
  100039:	41 b8 00 ff 00 00    	mov    $0xff00,%r8d
  10003f:	31 c9                	xor    %ecx,%ecx
  100041:	31 d2                	xor    %edx,%edx
  100043:	be 20 0e 10 00       	mov    $0x100e20,%esi
  100048:	48 89 df             	mov    %rbx,%rdi
  10004b:	e8 d0 0c 00 00       	call   100d20 <print_text>
  100050:	8b 43 10             	mov    0x10(%rbx),%eax
  100053:	c1 e8 02             	shr    $0x2,%eax
  100056:	0f af 43 0c          	imul   0xc(%rbx),%eax
  10005a:	85 c0                	test   %eax,%eax
  10005c:	74 3b                	je     100099 <kernel_main+0x79>
  10005e:	48 8b 0b             	mov    (%rbx),%rcx
  100061:	31 d2                	xor    %edx,%edx
  100063:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  10006a:	00 00 00 00 
  10006e:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  100075:	00 00 00 00 
  100079:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
  100080:	c7 04 91 00 00 00 00 	movl   $0x0,(%rcx,%rdx,4)
  100087:	8b 43 10             	mov    0x10(%rbx),%eax
  10008a:	48 83 c2 01          	add    $0x1,%rdx
  10008e:	c1 e8 02             	shr    $0x2,%eax
  100091:	0f af 43 0c          	imul   0xc(%rbx),%eax
  100095:	39 c2                	cmp    %eax,%edx
  100097:	72 e7                	jb     100080 <kernel_main+0x60>
  100099:	48 89 df             	mov    %rbx,%rdi
  10009c:	e8 2f 06 00 00       	call   1006d0 <cli>
  1000a1:	eb fe                	jmp    1000a1 <kernel_main+0x81>
  1000a3:	66 2e 0f 1f 84 00 00 	cs nopw 0x0(%rax,%rax,1)
  1000aa:	00 00 00 
  1000ad:	66 2e 0f 1f 84 00 00 	cs nopw 0x0(%rax,%rax,1)
  1000b4:	00 00 00 
  1000b7:	66 0f 1f 84 00 00 00 	nopw   0x0(%rax,%rax,1)
  1000be:	00 00 

00000000001000c0 <wait>:
  1000c0:	55                   	push   %rbp
  1000c1:	48 89 e5             	mov    %rsp,%rbp
  1000c4:	48 83 ec 18          	sub    $0x18,%rsp
  1000c8:	89 7d ec             	mov    %edi,-0x14(%rbp)
  1000cb:	c7 45 fc 00 00 00 00 	movl   $0x0,-0x4(%rbp)
  1000d2:	8b 55 fc             	mov    -0x4(%rbp),%edx
  1000d5:	8b 45 ec             	mov    -0x14(%rbp),%eax
  1000d8:	39 c2                	cmp    %eax,%edx
  1000da:	73 17                	jae    1000f3 <wait+0x33>
  1000dc:	0f 1f 40 00          	nopl   0x0(%rax)
  1000e0:	8b 45 fc             	mov    -0x4(%rbp),%eax
  1000e3:	83 c0 01             	add    $0x1,%eax
  1000e6:	89 45 fc             	mov    %eax,-0x4(%rbp)
  1000e9:	8b 55 fc             	mov    -0x4(%rbp),%edx
  1000ec:	8b 45 ec             	mov    -0x14(%rbp),%eax
  1000ef:	39 c2                	cmp    %eax,%edx
  1000f1:	72 ed                	jb     1000e0 <wait+0x20>
  1000f3:	c9                   	leave
  1000f4:	c3                   	ret
  1000f5:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  1000fc:	00 00 00 00 

0000000000100100 <read_ps2_key>:
  100100:	31 d2                	xor    %edx,%edx
  100102:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  100109:	00 00 00 00 
  10010d:	0f 1f 00             	nopl   (%rax)
  100110:	e4 64                	in     $0x64,%al
  100112:	a8 01                	test   $0x1,%al
  100114:	74 fa                	je     100110 <read_ps2_key+0x10>
  100116:	e4 60                	in     $0x60,%al
  100118:	3c e0                	cmp    $0xe0,%al
  10011a:	74 14                	je     100130 <read_ps2_key+0x30>
  10011c:	84 c0                	test   %al,%al
  10011e:	78 f0                	js     100110 <read_ps2_key+0x10>
  100120:	84 d2                	test   %dl,%dl
  100122:	74 1c                	je     100140 <read_ps2_key+0x40>
  100124:	83 e8 48             	sub    $0x48,%eax
  100127:	31 d2                	xor    %edx,%edx
  100129:	3c 08                	cmp    $0x8,%al
  10012b:	76 23                	jbe    100150 <read_ps2_key+0x50>
  10012d:	89 d0                	mov    %edx,%eax
  10012f:	c3                   	ret
  100130:	e4 64                	in     $0x64,%al
  100132:	a8 01                	test   $0x1,%al
  100134:	74 fa                	je     100130 <read_ps2_key+0x30>
  100136:	e4 60                	in     $0x60,%al
  100138:	ba 01 00 00 00       	mov    $0x1,%edx
  10013d:	eb dd                	jmp    10011c <read_ps2_key+0x1c>
  10013f:	90                   	nop
  100140:	0f b6 c0             	movzbl %al,%eax
  100143:	0f be 90 40 17 10 00 	movsbl 0x101740(%rax),%edx
  10014a:	89 d0                	mov    %edx,%eax
  10014c:	c3                   	ret
  10014d:	0f 1f 00             	nopl   (%rax)
  100150:	0f b6 c0             	movzbl %al,%eax
  100153:	8b 14 85 20 14 10 00 	mov    0x101420(,%rax,4),%edx
  10015a:	89 d0                	mov    %edx,%eax
  10015c:	c3                   	ret
  10015d:	0f 1f 00             	nopl   (%rax)

0000000000100160 <terminal_putentryat>:
  100160:	0f b6 15 8a 56 00 00 	movzbl 0x568a(%rip),%edx        # 1057f1 <t_row>
  100167:	0f b6 05 54 16 00 00 	movzbl 0x1654(%rip),%eax        # 1017c2 <t_pos_y>
  10016e:	40 0f be f7          	movsbl %dil,%esi
  100172:	44 8b 05 4b 16 00 00 	mov    0x164b(%rip),%r8d        # 1017c4 <t_color_char>
  100179:	48 8b 3d 60 56 00 00 	mov    0x5660(%rip),%rdi        # 1057e0 <fbcli>
  100180:	8d 0c d0             	lea    (%rax,%rdx,8),%ecx
  100183:	0f b6 05 39 16 00 00 	movzbl 0x1639(%rip),%eax        # 1017c3 <t_pos_x>
  10018a:	0f b6 15 5f 56 00 00 	movzbl 0x565f(%rip),%edx        # 1057f0 <t_col>
  100191:	8d 14 d0             	lea    (%rax,%rdx,8),%edx
  100194:	31 c0                	xor    %eax,%eax
  100196:	e9 25 0b 00 00       	jmp    100cc0 <draw_char>
  10019b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

00000000001001a0 <move_cursor>:
  1001a0:	c3                   	ret
  1001a1:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  1001a8:	00 00 00 00 
  1001ac:	0f 1f 40 00          	nopl   0x0(%rax)

00000000001001b0 <clear>:
  1001b0:	48 8b 0d 29 56 00 00 	mov    0x5629(%rip),%rcx        # 1057e0 <fbcli>
  1001b7:	8b 41 10             	mov    0x10(%rcx),%eax
  1001ba:	c1 e8 02             	shr    $0x2,%eax
  1001bd:	0f af 41 0c          	imul   0xc(%rcx),%eax
  1001c1:	85 c0                	test   %eax,%eax
  1001c3:	74 34                	je     1001f9 <clear+0x49>
  1001c5:	48 8b 31             	mov    (%rcx),%rsi
  1001c8:	31 d2                	xor    %edx,%edx
  1001ca:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  1001d1:	00 00 00 00 
  1001d5:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  1001dc:	00 00 00 00 
  1001e0:	c7 04 96 00 00 00 00 	movl   $0x0,(%rsi,%rdx,4)
  1001e7:	8b 41 10             	mov    0x10(%rcx),%eax
  1001ea:	48 83 c2 01          	add    $0x1,%rdx
  1001ee:	c1 e8 02             	shr    $0x2,%eax
  1001f1:	0f af 41 0c          	imul   0xc(%rcx),%eax
  1001f5:	39 c2                	cmp    %eax,%edx
  1001f7:	72 e7                	jb     1001e0 <clear+0x30>
  1001f9:	c6 05 f1 55 00 00 00 	movb   $0x0,0x55f1(%rip)        # 1057f1 <t_row>
  100200:	c6 05 e9 55 00 00 00 	movb   $0x0,0x55e9(%rip)        # 1057f0 <t_col>
  100207:	c3                   	ret
  100208:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  10020f:	00 

0000000000100210 <atoi>:
  100210:	0f b6 07             	movzbl (%rdi),%eax
  100213:	8d 50 d0             	lea    -0x30(%rax),%edx
  100216:	80 fa 09             	cmp    $0x9,%dl
  100219:	ba 00 00 00 00       	mov    $0x0,%edx
  10021e:	77 1b                	ja     10023b <atoi+0x2b>
  100220:	83 e8 30             	sub    $0x30,%eax
  100223:	8d 14 92             	lea    (%rdx,%rdx,4),%edx
  100226:	48 83 c7 01          	add    $0x1,%rdi
  10022a:	0f be c0             	movsbl %al,%eax
  10022d:	8d 14 50             	lea    (%rax,%rdx,2),%edx
  100230:	0f b6 07             	movzbl (%rdi),%eax
  100233:	8d 48 d0             	lea    -0x30(%rax),%ecx
  100236:	80 f9 09             	cmp    $0x9,%cl
  100239:	76 e5                	jbe    100220 <atoi+0x10>
  10023b:	89 d0                	mov    %edx,%eax
  10023d:	c3                   	ret
  10023e:	66 90                	xchg   %ax,%ax

0000000000100240 <print_char>:
  100240:	55                   	push   %rbp
  100241:	0f b6 05 a9 55 00 00 	movzbl 0x55a9(%rip),%eax        # 1057f1 <t_row>
  100248:	48 89 e5             	mov    %rsp,%rbp
  10024b:	40 80 ff 0a          	cmp    $0xa,%dil
  10024f:	74 4f                	je     1002a0 <print_char+0x60>
  100251:	0f b6 15 98 55 00 00 	movzbl 0x5598(%rip),%edx        # 1057f0 <t_col>
  100258:	89 d1                	mov    %edx,%ecx
  10025a:	c1 e1 03             	shl    $0x3,%ecx
  10025d:	66 3b 15 68 15 00 00 	cmp    0x1568(%rip),%dx        # 1017cc <t_colums>
  100264:	73 3a                	jae    1002a0 <print_char+0x60>
  100266:	0f b6 15 55 15 00 00 	movzbl 0x1555(%rip),%edx        # 1017c2 <t_pos_y>
  10026d:	0f b6 c0             	movzbl %al,%eax
  100270:	40 0f be f7          	movsbl %dil,%esi
  100274:	44 8b 05 49 15 00 00 	mov    0x1549(%rip),%r8d        # 1017c4 <t_color_char>
  10027b:	48 8b 3d 5e 55 00 00 	mov    0x555e(%rip),%rdi        # 1057e0 <fbcli>
  100282:	8d 04 c2             	lea    (%rdx,%rax,8),%eax
  100285:	0f b6 15 37 15 00 00 	movzbl 0x1537(%rip),%edx        # 1017c3 <t_pos_x>
  10028c:	01 ca                	add    %ecx,%edx
  10028e:	89 c1                	mov    %eax,%ecx
  100290:	31 c0                	xor    %eax,%eax
  100292:	e8 29 0a 00 00       	call   100cc0 <draw_char>
  100297:	80 05 52 55 00 00 01 	addb   $0x1,0x5552(%rip)        # 1057f0 <t_col>
  10029e:	5d                   	pop    %rbp
  10029f:	c3                   	ret
  1002a0:	83 c0 01             	add    $0x1,%eax
  1002a3:	c6 05 46 55 00 00 00 	movb   $0x0,0x5546(%rip)        # 1057f0 <t_col>
  1002aa:	31 c9                	xor    %ecx,%ecx
  1002ac:	88 05 3f 55 00 00    	mov    %al,0x553f(%rip)        # 1057f1 <t_row>
  1002b2:	eb b2                	jmp    100266 <print_char+0x26>
  1002b4:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  1002bb:	00 00 00 00 
  1002bf:	90                   	nop

00000000001002c0 <print>:
  1002c0:	0f be 37             	movsbl (%rdi),%esi
  1002c3:	40 84 f6             	test   %sil,%sil
  1002c6:	0f 84 94 00 00 00    	je     100360 <print+0xa0>
  1002cc:	55                   	push   %rbp
  1002cd:	48 89 e5             	mov    %rsp,%rbp
  1002d0:	53                   	push   %rbx
  1002d1:	48 89 fb             	mov    %rdi,%rbx
  1002d4:	48 83 ec 08          	sub    $0x8,%rsp
  1002d8:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  1002df:	00 
  1002e0:	0f b6 05 0a 55 00 00 	movzbl 0x550a(%rip),%eax        # 1057f1 <t_row>
  1002e7:	40 80 fe 0a          	cmp    $0xa,%sil
  1002eb:	74 15                	je     100302 <print+0x42>
  1002ed:	0f b6 15 fc 54 00 00 	movzbl 0x54fc(%rip),%edx        # 1057f0 <t_col>
  1002f4:	89 d1                	mov    %edx,%ecx
  1002f6:	c1 e1 03             	shl    $0x3,%ecx
  1002f9:	66 3b 15 cc 14 00 00 	cmp    0x14cc(%rip),%dx        # 1017cc <t_colums>
  100300:	72 12                	jb     100314 <print+0x54>
  100302:	83 c0 01             	add    $0x1,%eax
  100305:	c6 05 e4 54 00 00 00 	movb   $0x0,0x54e4(%rip)        # 1057f0 <t_col>
  10030c:	31 c9                	xor    %ecx,%ecx
  10030e:	88 05 dd 54 00 00    	mov    %al,0x54dd(%rip)        # 1057f1 <t_row>
  100314:	0f b6 15 a7 14 00 00 	movzbl 0x14a7(%rip),%edx        # 1017c2 <t_pos_y>
  10031b:	0f b6 c0             	movzbl %al,%eax
  10031e:	48 83 c3 01          	add    $0x1,%rbx
  100322:	44 8b 05 9b 14 00 00 	mov    0x149b(%rip),%r8d        # 1017c4 <t_color_char>
  100329:	48 8b 3d b0 54 00 00 	mov    0x54b0(%rip),%rdi        # 1057e0 <fbcli>
  100330:	8d 04 c2             	lea    (%rdx,%rax,8),%eax
  100333:	0f b6 15 89 14 00 00 	movzbl 0x1489(%rip),%edx        # 1017c3 <t_pos_x>
  10033a:	01 ca                	add    %ecx,%edx
  10033c:	89 c1                	mov    %eax,%ecx
  10033e:	31 c0                	xor    %eax,%eax
  100340:	e8 7b 09 00 00       	call   100cc0 <draw_char>
  100345:	80 05 a4 54 00 00 01 	addb   $0x1,0x54a4(%rip)        # 1057f0 <t_col>
  10034c:	0f be 33             	movsbl (%rbx),%esi
  10034f:	40 84 f6             	test   %sil,%sil
  100352:	75 8c                	jne    1002e0 <print+0x20>
  100354:	48 8b 5d f8          	mov    -0x8(%rbp),%rbx
  100358:	c9                   	leave
  100359:	c3                   	ret
  10035a:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
  100360:	c3                   	ret
  100361:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  100368:	00 00 00 00 
  10036c:	0f 1f 40 00          	nopl   0x0(%rax)

0000000000100370 <print_hex>:
  100370:	55                   	push   %rbp
  100371:	b8 30 78 00 00       	mov    $0x7830,%eax
  100376:	48 89 e5             	mov    %rsp,%rbp
  100379:	48 83 ec 30          	sub    $0x30,%rsp
  10037d:	66 0f 6f 05 cb 10 00 	movdqa 0x10cb(%rip),%xmm0        # 101450 <CSWTCH.40+0x30>
  100384:	00 
  100385:	66 89 45 da          	mov    %ax,-0x26(%rbp)
  100389:	89 f8                	mov    %edi,%eax
  10038b:	83 e7 0f             	and    $0xf,%edi
  10038e:	c0 e8 04             	shr    $0x4,%al
  100391:	0f 29 45 e0          	movaps %xmm0,-0x20(%rbp)
  100395:	83 e0 0f             	and    $0xf,%eax
  100398:	c6 45 f0 00          	movb   $0x0,-0x10(%rbp)
  10039c:	0f b6 44 05 e0       	movzbl -0x20(%rbp,%rax,1),%eax
  1003a1:	c6 45 de 00          	movb   $0x0,-0x22(%rbp)
  1003a5:	88 45 dc             	mov    %al,-0x24(%rbp)
  1003a8:	0f b6 44 3d e0       	movzbl -0x20(%rbp,%rdi,1),%eax
  1003ad:	48 8d 7d da          	lea    -0x26(%rbp),%rdi
  1003b1:	88 45 dd             	mov    %al,-0x23(%rbp)
  1003b4:	e8 07 ff ff ff       	call   1002c0 <print>
  1003b9:	c9                   	leave
  1003ba:	c3                   	ret
  1003bb:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

00000000001003c0 <read_line>:
  1003c0:	55                   	push   %rbp
  1003c1:	48 89 e5             	mov    %rsp,%rbp
  1003c4:	41 57                	push   %r15
  1003c6:	41 56                	push   %r14
  1003c8:	41 55                	push   %r13
  1003ca:	41 54                	push   %r12
  1003cc:	53                   	push   %rbx
  1003cd:	48 89 fb             	mov    %rdi,%rbx
  1003d0:	48 83 ec 48          	sub    $0x48,%rsp
  1003d4:	83 fe 01             	cmp    $0x1,%esi
  1003d7:	0f 8e dc 02 00 00    	jle    1006b9 <read_line+0x2f9>
  1003dd:	8d 46 ff             	lea    -0x1(%rsi),%eax
  1003e0:	44 8b 2d 29 56 00 00 	mov    0x5629(%rip),%r13d        # 105a10 <bufer_history>
  1003e7:	44 0f b7 35 55 10 00 	movzwl 0x1055(%rip),%r14d        # 101444 <CSWTCH.40+0x24>
  1003ee:	00 
  1003ef:	45 31 ff             	xor    %r15d,%r15d
  1003f2:	89 45 98             	mov    %eax,-0x68(%rbp)
  1003f5:	e9 ae 00 00 00       	jmp    1004a8 <read_line+0xe8>
  1003fa:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
  100400:	41 80 fc 81          	cmp    $0x81,%r12b
  100404:	0f 84 ae 01 00 00    	je     1005b8 <read_line+0x1f8>
  10040a:	41 83 fc 08          	cmp    $0x8,%r12d
  10040e:	0f 84 14 02 00 00    	je     100628 <read_line+0x268>
  100414:	49 63 d7             	movslq %r15d,%rdx
  100417:	48 01 da             	add    %rbx,%rdx
  10041a:	41 83 fc 1c          	cmp    $0x1c,%r12d
  10041e:	77 11                	ja     100431 <read_line+0x71>
  100420:	48 c7 c1 ff db ff ef 	mov    $0xffffffffefffdbff,%rcx
  100427:	4c 0f a3 e1          	bt     %r12,%rcx
  10042b:	0f 83 59 02 00 00    	jae    10068a <read_line+0x2ca>
  100431:	44 88 22             	mov    %r12b,(%rdx)
  100434:	0f b6 35 b5 53 00 00 	movzbl 0x53b5(%rip),%esi        # 1057f0 <t_col>
  10043b:	41 83 c7 01          	add    $0x1,%r15d
  10043f:	0f b6 15 ab 53 00 00 	movzbl 0x53ab(%rip),%edx        # 1057f1 <t_row>
  100446:	89 f1                	mov    %esi,%ecx
  100448:	c1 e1 03             	shl    $0x3,%ecx
  10044b:	66 3b 35 7a 13 00 00 	cmp    0x137a(%rip),%si        # 1017cc <t_colums>
  100452:	72 12                	jb     100466 <read_line+0xa6>
  100454:	83 c2 01             	add    $0x1,%edx
  100457:	c6 05 92 53 00 00 00 	movb   $0x0,0x5392(%rip)        # 1057f0 <t_col>
  10045e:	31 c9                	xor    %ecx,%ecx
  100460:	88 15 8b 53 00 00    	mov    %dl,0x538b(%rip)        # 1057f1 <t_row>
  100466:	0f b6 35 55 13 00 00 	movzbl 0x1355(%rip),%esi        # 1017c2 <t_pos_y>
  10046d:	0f b6 d2             	movzbl %dl,%edx
  100470:	44 8b 05 4d 13 00 00 	mov    0x134d(%rip),%r8d        # 1017c4 <t_color_char>
  100477:	31 c0                	xor    %eax,%eax
  100479:	8d 3c d6             	lea    (%rsi,%rdx,8),%edi
  10047c:	0f b6 15 40 13 00 00 	movzbl 0x1340(%rip),%edx        # 1017c3 <t_pos_x>
  100483:	41 0f be f4          	movsbl %r12b,%esi
  100487:	01 ca                	add    %ecx,%edx
  100489:	89 f9                	mov    %edi,%ecx
  10048b:	48 8b 3d 4e 53 00 00 	mov    0x534e(%rip),%rdi        # 1057e0 <fbcli>
  100492:	e8 29 08 00 00       	call   100cc0 <draw_char>
  100497:	80 05 52 53 00 00 01 	addb   $0x1,0x5352(%rip)        # 1057f0 <t_col>
  10049e:	44 39 7d 98          	cmp    %r15d,-0x68(%rbp)
  1004a2:	0f 8e d6 00 00 00    	jle    10057e <read_line+0x1be>
  1004a8:	e8 53 fc ff ff       	call   100100 <read_ps2_key>
  1004ad:	48 8d 7d aa          	lea    -0x56(%rbp),%rdi
  1004b1:	c6 45 c0 00          	movb   $0x0,-0x40(%rbp)
  1004b5:	66 0f 6f 05 93 0f 00 	movdqa 0xf93(%rip),%xmm0        # 101450 <CSWTCH.40+0x30>
  1004bc:	00 
  1004bd:	89 c2                	mov    %eax,%edx
  1004bf:	41 89 c4             	mov    %eax,%r12d
  1004c2:	66 44 89 75 aa       	mov    %r14w,-0x56(%rbp)
  1004c7:	c0 ea 04             	shr    $0x4,%dl
  1004ca:	0f 29 45 b0          	movaps %xmm0,-0x50(%rbp)
  1004ce:	83 e2 0f             	and    $0xf,%edx
  1004d1:	c6 45 ae 00          	movb   $0x0,-0x52(%rbp)
  1004d5:	0f b6 54 15 b0       	movzbl -0x50(%rbp,%rdx,1),%edx
  1004da:	88 55 ac             	mov    %dl,-0x54(%rbp)
  1004dd:	89 c2                	mov    %eax,%edx
  1004df:	83 e2 0f             	and    $0xf,%edx
  1004e2:	0f b6 54 15 b0       	movzbl -0x50(%rbp,%rdx,1),%edx
  1004e7:	88 55 ad             	mov    %dl,-0x53(%rbp)
  1004ea:	e8 d1 fd ff ff       	call   1002c0 <print>
  1004ef:	41 80 fc 80          	cmp    $0x80,%r12b
  1004f3:	0f 85 07 ff ff ff    	jne    100400 <read_line+0x40>
  1004f9:	45 85 ed             	test   %r13d,%r13d
  1004fc:	74 a0                	je     10049e <read_line+0xde>
  1004fe:	41 83 ed 01          	sub    $0x1,%r13d
  100502:	c6 05 e7 52 00 00 03 	movb   $0x3,0x52e7(%rip)        # 1057f0 <t_col>
  100509:	31 f6                	xor    %esi,%esi
  10050b:	49 63 fd             	movslq %r13d,%rdi
  10050e:	48 8b 0c fd 80 58 10 	mov    0x105880(,%rdi,8),%rcx
  100515:	00 
  100516:	80 39 00             	cmpb   $0x0,(%rcx)
  100519:	74 40                	je     10055b <read_line+0x19b>
  10051b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  100520:	31 c0                	xor    %eax,%eax
  100522:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  100529:	00 00 00 00 
  10052d:	0f 1f 00             	nopl   (%rax)
  100530:	83 c0 01             	add    $0x1,%eax
  100533:	0f b6 d0             	movzbl %al,%edx
  100536:	80 3c 11 00          	cmpb   $0x0,(%rcx,%rdx,1)
  10053a:	75 f4                	jne    100530 <read_line+0x170>
  10053c:	0f b6 c0             	movzbl %al,%eax
  10053f:	39 f0                	cmp    %esi,%eax
  100541:	7e 18                	jle    10055b <read_line+0x19b>
  100543:	0f b6 04 31          	movzbl (%rcx,%rsi,1),%eax
  100547:	88 04 33             	mov    %al,(%rbx,%rsi,1)
  10054a:	48 8b 0c fd 80 58 10 	mov    0x105880(,%rdi,8),%rcx
  100551:	00 
  100552:	48 83 c6 01          	add    $0x1,%rsi
  100556:	80 39 00             	cmpb   $0x0,(%rcx)
  100559:	75 c5                	jne    100520 <read_line+0x160>
  10055b:	41 89 f7             	mov    %esi,%r15d
  10055e:	48 63 f6             	movslq %esi,%rsi
  100561:	48 89 df             	mov    %rbx,%rdi
  100564:	c6 04 33 00          	movb   $0x0,(%rbx,%rsi,1)
  100568:	c6 05 81 52 00 00 03 	movb   $0x3,0x5281(%rip)        # 1057f0 <t_col>
  10056f:	e8 4c fd ff ff       	call   1002c0 <print>
  100574:	44 39 7d 98          	cmp    %r15d,-0x68(%rbp)
  100578:	0f 8f 2a ff ff ff    	jg     1004a8 <read_line+0xe8>
  10057e:	4d 63 ff             	movslq %r15d,%r15
  100581:	4a 8d 14 3b          	lea    (%rbx,%r15,1),%rdx
  100585:	bf 30 0e 10 00       	mov    $0x100e30,%edi
  10058a:	48 89 55 98          	mov    %rdx,-0x68(%rbp)
  10058e:	e8 2d fd ff ff       	call   1002c0 <print>
  100593:	48 8b 55 98          	mov    -0x68(%rbp),%rdx
  100597:	bf 30 0e 10 00       	mov    $0x100e30,%edi
  10059c:	c6 02 00             	movb   $0x0,(%rdx)
  10059f:	48 83 c4 48          	add    $0x48,%rsp
  1005a3:	5b                   	pop    %rbx
  1005a4:	41 5c                	pop    %r12
  1005a6:	41 5d                	pop    %r13
  1005a8:	41 5e                	pop    %r14
  1005aa:	41 5f                	pop    %r15
  1005ac:	5d                   	pop    %rbp
  1005ad:	e9 0e fd ff ff       	jmp    1002c0 <print>
  1005b2:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
  1005b8:	8b 05 52 54 00 00    	mov    0x5452(%rip),%eax        # 105a10 <bufer_history>
  1005be:	83 e8 01             	sub    $0x1,%eax
  1005c1:	44 39 e8             	cmp    %r13d,%eax
  1005c4:	0f 84 d4 fe ff ff    	je     10049e <read_line+0xde>
  1005ca:	c6 05 1f 52 00 00 03 	movb   $0x3,0x521f(%rip)        # 1057f0 <t_col>
  1005d1:	41 83 c5 01          	add    $0x1,%r13d
  1005d5:	31 f6                	xor    %esi,%esi
  1005d7:	49 63 fd             	movslq %r13d,%rdi
  1005da:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
  1005e0:	48 8b 0c fd 80 58 10 	mov    0x105880(,%rdi,8),%rcx
  1005e7:	00 
  1005e8:	80 39 00             	cmpb   $0x0,(%rcx)
  1005eb:	0f 84 6a ff ff ff    	je     10055b <read_line+0x19b>
  1005f1:	31 c0                	xor    %eax,%eax
  1005f3:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  1005fa:	00 00 00 00 
  1005fe:	66 90                	xchg   %ax,%ax
  100600:	83 c0 01             	add    $0x1,%eax
  100603:	0f b6 d0             	movzbl %al,%edx
  100606:	80 3c 11 00          	cmpb   $0x0,(%rcx,%rdx,1)
  10060a:	75 f4                	jne    100600 <read_line+0x240>
  10060c:	0f b6 c0             	movzbl %al,%eax
  10060f:	39 f0                	cmp    %esi,%eax
  100611:	0f 8e 44 ff ff ff    	jle    10055b <read_line+0x19b>
  100617:	0f b6 04 31          	movzbl (%rcx,%rsi,1),%eax
  10061b:	88 04 33             	mov    %al,(%rbx,%rsi,1)
  10061e:	48 83 c6 01          	add    $0x1,%rsi
  100622:	eb bc                	jmp    1005e0 <read_line+0x220>
  100624:	0f 1f 40 00          	nopl   0x0(%rax)
  100628:	45 85 ff             	test   %r15d,%r15d
  10062b:	0f 8e 77 fe ff ff    	jle    1004a8 <read_line+0xe8>
  100631:	0f b6 05 b8 51 00 00 	movzbl 0x51b8(%rip),%eax        # 1057f0 <t_col>
  100638:	84 c0                	test   %al,%al
  10063a:	0f 84 5e fe ff ff    	je     10049e <read_line+0xde>
  100640:	0f b6 15 7b 11 00 00 	movzbl 0x117b(%rip),%edx        # 1017c2 <t_pos_y>
  100647:	83 e8 01             	sub    $0x1,%eax
  10064a:	0f b6 0d a0 51 00 00 	movzbl 0x51a0(%rip),%ecx        # 1057f1 <t_row>
  100651:	be 20 00 00 00       	mov    $0x20,%esi
  100656:	88 05 94 51 00 00    	mov    %al,0x5194(%rip)        # 1057f0 <t_col>
  10065c:	44 8b 05 61 11 00 00 	mov    0x1161(%rip),%r8d        # 1017c4 <t_color_char>
  100663:	0f b6 c0             	movzbl %al,%eax
  100666:	41 83 ef 01          	sub    $0x1,%r15d
  10066a:	8d 0c ca             	lea    (%rdx,%rcx,8),%ecx
  10066d:	0f b6 15 4f 11 00 00 	movzbl 0x114f(%rip),%edx        # 1017c3 <t_pos_x>
  100674:	48 8b 3d 65 51 00 00 	mov    0x5165(%rip),%rdi        # 1057e0 <fbcli>
  10067b:	8d 14 c2             	lea    (%rdx,%rax,8),%edx
  10067e:	31 c0                	xor    %eax,%eax
  100680:	e8 3b 06 00 00       	call   100cc0 <draw_char>
  100685:	e9 14 fe ff ff       	jmp    10049e <read_line+0xde>
  10068a:	bf 2e 0e 10 00       	mov    $0x100e2e,%edi
  10068f:	48 89 55 98          	mov    %rdx,-0x68(%rbp)
  100693:	e8 28 fc ff ff       	call   1002c0 <print>
  100698:	bf 2e 0e 10 00       	mov    $0x100e2e,%edi
  10069d:	80 05 4d 51 00 00 01 	addb   $0x1,0x514d(%rip)        # 1057f1 <t_row>
  1006a4:	c6 05 45 51 00 00 00 	movb   $0x0,0x5145(%rip)        # 1057f0 <t_col>
  1006ab:	e8 10 fc ff ff       	call   1002c0 <print>
  1006b0:	48 8b 55 98          	mov    -0x68(%rbp),%rdx
  1006b4:	e9 cc fe ff ff       	jmp    100585 <read_line+0x1c5>
  1006b9:	48 89 fa             	mov    %rdi,%rdx
  1006bc:	e9 c4 fe ff ff       	jmp    100585 <read_line+0x1c5>
  1006c1:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  1006c8:	00 00 00 00 
  1006cc:	0f 1f 40 00          	nopl   0x0(%rax)

00000000001006d0 <cli>:
  1006d0:	55                   	push   %rbp
  1006d1:	48 89 e5             	mov    %rsp,%rbp
  1006d4:	41 54                	push   %r12
  1006d6:	49 89 fc             	mov    %rdi,%r12
  1006d9:	53                   	push   %rbx
  1006da:	bb 80 58 10 00       	mov    $0x105880,%ebx
  1006df:	48 83 ec 70          	sub    $0x70,%rsp
  1006e3:	eb 10                	jmp    1006f5 <cli+0x25>
  1006e5:	0f 1f 00             	nopl   (%rax)
  1006e8:	48 83 c3 08          	add    $0x8,%rbx
  1006ec:	48 81 fb 20 59 10 00 	cmp    $0x105920,%rbx
  1006f3:	74 2b                	je     100720 <cli+0x50>
  1006f5:	bf 64 00 00 00       	mov    $0x64,%edi
  1006fa:	e8 a1 06 00 00       	call   100da0 <kmalloc>
  1006ff:	48 89 03             	mov    %rax,(%rbx)
  100702:	48 83 3d 76 51 00 00 	cmpq   $0x0,0x5176(%rip)        # 105880 <key_history>
  100709:	00 
  10070a:	75 dc                	jne    1006e8 <cli+0x18>
  10070c:	bf 55 0e 10 00       	mov    $0x100e55,%edi
  100711:	e8 aa fb ff ff       	call   1002c0 <print>
  100716:	eb fe                	jmp    100716 <cli+0x46>
  100718:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  10071f:	00 
  100720:	41 8b 44 24 10       	mov    0x10(%r12),%eax
  100725:	4c 89 25 b4 50 00 00 	mov    %r12,0x50b4(%rip)        # 1057e0 <fbcli>
  10072c:	c1 e8 02             	shr    $0x2,%eax
  10072f:	41 0f af 44 24 0c    	imul   0xc(%r12),%eax
  100735:	85 c0                	test   %eax,%eax
  100737:	74 24                	je     10075d <cli+0x8d>
  100739:	49 8b 0c 24          	mov    (%r12),%rcx
  10073d:	31 d2                	xor    %edx,%edx
  10073f:	90                   	nop
  100740:	c7 04 91 00 00 00 00 	movl   $0x0,(%rcx,%rdx,4)
  100747:	41 8b 44 24 10       	mov    0x10(%r12),%eax
  10074c:	48 83 c2 01          	add    $0x1,%rdx
  100750:	c1 e8 02             	shr    $0x2,%eax
  100753:	41 0f af 44 24 0c    	imul   0xc(%r12),%eax
  100759:	39 c2                	cmp    %eax,%edx
  10075b:	72 e3                	jb     100740 <cli+0x70>
  10075d:	bf b8 12 10 00       	mov    $0x1012b8,%edi
  100762:	c6 05 88 50 00 00 00 	movb   $0x0,0x5088(%rip)        # 1057f1 <t_row>
  100769:	c6 05 80 50 00 00 00 	movb   $0x0,0x5080(%rip)        # 1057f0 <t_col>
  100770:	c7 05 4a 10 00 00 52 	movl   $0x32a852,0x104a(%rip)        # 1017c4 <t_color_char>
  100777:	a8 32 00 
  10077a:	e8 41 fb ff ff       	call   1002c0 <print>
  10077f:	c7 05 3b 10 00 00 ff 	movl   $0xffffff,0x103b(%rip)        # 1017c4 <t_color_char>
  100786:	ff ff 00 
  100789:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
  100790:	bf 5b 0e 10 00       	mov    $0x100e5b,%edi
  100795:	e8 26 fb ff ff       	call   1002c0 <print>
  10079a:	be 64 00 00 00       	mov    $0x64,%esi
  10079f:	48 8d 7d 80          	lea    -0x80(%rbp),%rdi
  1007a3:	e8 18 fc ff ff       	call   1003c0 <read_line>
  1007a8:	0f b6 75 80          	movzbl -0x80(%rbp),%esi
  1007ac:	40 84 f6             	test   %sil,%sil
  1007af:	0f 84 61 01 00 00    	je     100916 <cli+0x246>
  1007b5:	48 63 05 54 52 00 00 	movslq 0x5254(%rip),%rax        # 105a10 <bufer_history>
  1007bc:	83 f8 13             	cmp    $0x13,%eax
  1007bf:	0f 8e a3 01 00 00    	jle    100968 <cli+0x298>
  1007c5:	89 f0                	mov    %esi,%eax
  1007c7:	ba 32 0e 10 00       	mov    $0x100e32,%edx
  1007cc:	48 8d 4d 80          	lea    -0x80(%rbp),%rcx
  1007d0:	eb 22                	jmp    1007f4 <cli+0x124>
  1007d2:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  1007d9:	00 00 00 00 
  1007dd:	0f 1f 00             	nopl   (%rax)
  1007e0:	0f b6 41 01          	movzbl 0x1(%rcx),%eax
  1007e4:	48 83 c1 01          	add    $0x1,%rcx
  1007e8:	48 83 c2 01          	add    $0x1,%rdx
  1007ec:	84 c0                	test   %al,%al
  1007ee:	0f 84 34 01 00 00    	je     100928 <cli+0x258>
  1007f4:	38 02                	cmp    %al,(%rdx)
  1007f6:	74 e8                	je     1007e0 <cli+0x110>
  1007f8:	3a 02                	cmp    (%rdx),%al
  1007fa:	0f 84 32 01 00 00    	je     100932 <cli+0x262>
  100800:	89 f0                	mov    %esi,%eax
  100802:	ba 37 0e 10 00       	mov    $0x100e37,%edx
  100807:	48 8d 4d 80          	lea    -0x80(%rbp),%rcx
  10080b:	eb 27                	jmp    100834 <cli+0x164>
  10080d:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  100814:	00 00 00 00 
  100818:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  10081f:	00 
  100820:	0f b6 41 01          	movzbl 0x1(%rcx),%eax
  100824:	48 83 c1 01          	add    $0x1,%rcx
  100828:	48 83 c2 01          	add    $0x1,%rdx
  10082c:	84 c0                	test   %al,%al
  10082e:	0f 84 14 01 00 00    	je     100948 <cli+0x278>
  100834:	38 02                	cmp    %al,(%rdx)
  100836:	74 e8                	je     100820 <cli+0x150>
  100838:	38 02                	cmp    %al,(%rdx)
  10083a:	0f 84 12 01 00 00    	je     100952 <cli+0x282>
  100840:	89 f0                	mov    %esi,%eax
  100842:	ba 3d 0e 10 00       	mov    $0x100e3d,%edx
  100847:	48 8d 4d 80          	lea    -0x80(%rbp),%rcx
  10084b:	eb 27                	jmp    100874 <cli+0x1a4>
  10084d:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  100854:	00 00 00 00 
  100858:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  10085f:	00 
  100860:	0f b6 41 01          	movzbl 0x1(%rcx),%eax
  100864:	48 83 c1 01          	add    $0x1,%rcx
  100868:	48 83 c2 01          	add    $0x1,%rdx
  10086c:	84 c0                	test   %al,%al
  10086e:	0f 84 6c 01 00 00    	je     1009e0 <cli+0x310>
  100874:	38 02                	cmp    %al,(%rdx)
  100876:	74 e8                	je     100860 <cli+0x190>
  100878:	38 02                	cmp    %al,(%rdx)
  10087a:	0f 84 6a 01 00 00    	je     1009ea <cli+0x31a>
  100880:	89 f0                	mov    %esi,%eax
  100882:	ba 46 0e 10 00       	mov    $0x100e46,%edx
  100887:	48 8d 4d 80          	lea    -0x80(%rbp),%rcx
  10088b:	eb 27                	jmp    1008b4 <cli+0x1e4>
  10088d:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  100894:	00 00 00 00 
  100898:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  10089f:	00 
  1008a0:	0f b6 41 01          	movzbl 0x1(%rcx),%eax
  1008a4:	48 83 c1 01          	add    $0x1,%rcx
  1008a8:	48 83 c2 01          	add    $0x1,%rdx
  1008ac:	84 c0                	test   %al,%al
  1008ae:	0f 84 5c 01 00 00    	je     100a10 <cli+0x340>
  1008b4:	38 02                	cmp    %al,(%rdx)
  1008b6:	74 e8                	je     1008a0 <cli+0x1d0>
  1008b8:	b9 4c 0e 10 00       	mov    $0x100e4c,%ecx
  1008bd:	48 8d 7d 80          	lea    -0x80(%rbp),%rdi
  1008c1:	38 02                	cmp    %al,(%rdx)
  1008c3:	75 30                	jne    1008f5 <cli+0x225>
  1008c5:	e9 59 01 00 00       	jmp    100a23 <cli+0x353>
  1008ca:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  1008d1:	00 00 00 00 
  1008d5:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  1008dc:	00 00 00 00 
  1008e0:	0f b6 77 01          	movzbl 0x1(%rdi),%esi
  1008e4:	48 83 c7 01          	add    $0x1,%rdi
  1008e8:	48 83 c1 01          	add    $0x1,%rcx
  1008ec:	40 84 f6             	test   %sil,%sil
  1008ef:	0f 84 77 01 00 00    	je     100a6c <cli+0x39c>
  1008f5:	40 38 31             	cmp    %sil,(%rcx)
  1008f8:	74 e6                	je     1008e0 <cli+0x210>
  1008fa:	40 38 31             	cmp    %sil,(%rcx)
  1008fd:	0f 84 74 01 00 00    	je     100a77 <cli+0x3a7>
  100903:	bf 89 0e 10 00       	mov    $0x100e89,%edi
  100908:	e8 b3 f9 ff ff       	call   1002c0 <print>
  10090d:	48 8d 7d 80          	lea    -0x80(%rbp),%rdi
  100911:	e8 aa f9 ff ff       	call   1002c0 <print>
  100916:	bf 67 0e 10 00       	mov    $0x100e67,%edi
  10091b:	e8 a0 f9 ff ff       	call   1002c0 <print>
  100920:	e9 6b fe ff ff       	jmp    100790 <cli+0xc0>
  100925:	0f 1f 00             	nopl   (%rax)
  100928:	31 c0                	xor    %eax,%eax
  10092a:	3a 02                	cmp    (%rdx),%al
  10092c:	0f 85 ce fe ff ff    	jne    100800 <cli+0x130>
  100932:	48 83 c4 70          	add    $0x70,%rsp
  100936:	bf 5e 0e 10 00       	mov    $0x100e5e,%edi
  10093b:	5b                   	pop    %rbx
  10093c:	41 5c                	pop    %r12
  10093e:	5d                   	pop    %rbp
  10093f:	e9 7c f9 ff ff       	jmp    1002c0 <print>
  100944:	0f 1f 40 00          	nopl   0x0(%rax)
  100948:	31 c0                	xor    %eax,%eax
  10094a:	38 02                	cmp    %al,(%rdx)
  10094c:	0f 85 ee fe ff ff    	jne    100840 <cli+0x170>
  100952:	bf 69 0e 10 00       	mov    $0x100e69,%edi
  100957:	e8 64 f9 ff ff       	call   1002c0 <print>
  10095c:	e9 2f fe ff ff       	jmp    100790 <cli+0xc0>
  100961:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
  100968:	31 d2                	xor    %edx,%edx
  10096a:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
  100970:	83 c2 01             	add    $0x1,%edx
  100973:	0f b6 ca             	movzbl %dl,%ecx
  100976:	80 7c 0d 80 00       	cmpb   $0x0,-0x80(%rbp,%rcx,1)
  10097b:	75 f3                	jne    100970 <cli+0x2a0>
  10097d:	41 b8 63 00 00 00    	mov    $0x63,%r8d
  100983:	44 38 c2             	cmp    %r8b,%dl
  100986:	44 0f 46 c2          	cmovbe %edx,%r8d
  10098a:	84 d2                	test   %dl,%dl
  10098c:	74 31                	je     1009bf <cli+0x2ef>
  10098e:	41 0f b6 f8          	movzbl %r8b,%edi
  100992:	31 d2                	xor    %edx,%edx
  100994:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  10099b:	00 00 00 00 
  10099f:	90                   	nop
  1009a0:	48 8b 04 c5 80 58 10 	mov    0x105880(,%rax,8),%rax
  1009a7:	00 
  1009a8:	0f b6 4c 15 80       	movzbl -0x80(%rbp,%rdx,1),%ecx
  1009ad:	88 0c 10             	mov    %cl,(%rax,%rdx,1)
  1009b0:	48 83 c2 01          	add    $0x1,%rdx
  1009b4:	48 63 05 55 50 00 00 	movslq 0x5055(%rip),%rax        # 105a10 <bufer_history>
  1009bb:	39 d7                	cmp    %edx,%edi
  1009bd:	7f e1                	jg     1009a0 <cli+0x2d0>
  1009bf:	48 8b 04 c5 80 58 10 	mov    0x105880(,%rax,8),%rax
  1009c6:	00 
  1009c7:	45 0f b6 c0          	movzbl %r8b,%r8d
  1009cb:	42 c6 04 00 00       	movb   $0x0,(%rax,%r8,1)
  1009d0:	83 05 39 50 00 00 01 	addl   $0x1,0x5039(%rip)        # 105a10 <bufer_history>
  1009d7:	e9 e9 fd ff ff       	jmp    1007c5 <cli+0xf5>
  1009dc:	0f 1f 40 00          	nopl   0x0(%rax)
  1009e0:	31 c0                	xor    %eax,%eax
  1009e2:	38 02                	cmp    %al,(%rdx)
  1009e4:	0f 85 96 fe ff ff    	jne    100880 <cli+0x1b0>
  1009ea:	c7 05 d0 0d 00 00 52 	movl   $0x32a852,0xdd0(%rip)        # 1017c4 <t_color_char>
  1009f1:	a8 32 00 
  1009f4:	bf b8 12 10 00       	mov    $0x1012b8,%edi
  1009f9:	e8 c2 f8 ff ff       	call   1002c0 <print>
  1009fe:	c7 05 bc 0d 00 00 ff 	movl   $0xffffff,0xdbc(%rip)        # 1017c4 <t_color_char>
  100a05:	ff ff 00 
  100a08:	e9 83 fd ff ff       	jmp    100790 <cli+0xc0>
  100a0d:	0f 1f 00             	nopl   (%rax)
  100a10:	31 c0                	xor    %eax,%eax
  100a12:	b9 4c 0e 10 00       	mov    $0x100e4c,%ecx
  100a17:	48 8d 7d 80          	lea    -0x80(%rbp),%rdi
  100a1b:	38 02                	cmp    %al,(%rdx)
  100a1d:	0f 85 d2 fe ff ff    	jne    1008f5 <cli+0x225>
  100a23:	48 8b 0d b6 4d 00 00 	mov    0x4db6(%rip),%rcx        # 1057e0 <fbcli>
  100a2a:	8b 41 10             	mov    0x10(%rcx),%eax
  100a2d:	c1 e8 02             	shr    $0x2,%eax
  100a30:	0f af 41 0c          	imul   0xc(%rcx),%eax
  100a34:	85 c0                	test   %eax,%eax
  100a36:	74 21                	je     100a59 <cli+0x389>
  100a38:	48 8b 31             	mov    (%rcx),%rsi
  100a3b:	31 d2                	xor    %edx,%edx
  100a3d:	0f 1f 00             	nopl   (%rax)
  100a40:	c7 04 96 00 00 00 00 	movl   $0x0,(%rsi,%rdx,4)
  100a47:	8b 41 10             	mov    0x10(%rcx),%eax
  100a4a:	48 83 c2 01          	add    $0x1,%rdx
  100a4e:	c1 e8 02             	shr    $0x2,%eax
  100a51:	0f af 41 0c          	imul   0xc(%rcx),%eax
  100a55:	39 c2                	cmp    %eax,%edx
  100a57:	72 e7                	jb     100a40 <cli+0x370>
  100a59:	c6 05 91 4d 00 00 00 	movb   $0x0,0x4d91(%rip)        # 1057f1 <t_row>
  100a60:	c6 05 89 4d 00 00 00 	movb   $0x0,0x4d89(%rip)        # 1057f0 <t_col>
  100a67:	e9 24 fd ff ff       	jmp    100790 <cli+0xc0>
  100a6c:	31 f6                	xor    %esi,%esi
  100a6e:	40 38 31             	cmp    %sil,(%rcx)
  100a71:	0f 85 8c fe ff ff    	jne    100903 <cli+0x233>
  100a77:	bf 72 0e 10 00       	mov    $0x100e72,%edi
  100a7c:	e8 3f f8 ff ff       	call   1002c0 <print>
  100a81:	be 64 00 00 00       	mov    $0x64,%esi
  100a86:	48 8d 7d 80          	lea    -0x80(%rbp),%rdi
  100a8a:	e8 31 f9 ff ff       	call   1003c0 <read_line>
  100a8f:	0f b6 45 80          	movzbl -0x80(%rbp),%eax
  100a93:	8d 50 d0             	lea    -0x30(%rax),%edx
  100a96:	80 fa 09             	cmp    $0x9,%dl
  100a99:	77 4c                	ja     100ae7 <cli+0x417>
  100a9b:	48 8d 4d 80          	lea    -0x80(%rbp),%rcx
  100a9f:	31 d2                	xor    %edx,%edx
  100aa1:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  100aa8:	00 00 00 00 
  100aac:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  100ab3:	00 00 00 00 
  100ab7:	66 0f 1f 84 00 00 00 	nopw   0x0(%rax,%rax,1)
  100abe:	00 00 
  100ac0:	83 e8 30             	sub    $0x30,%eax
  100ac3:	8d 14 92             	lea    (%rdx,%rdx,4),%edx
  100ac6:	48 83 c1 01          	add    $0x1,%rcx
  100aca:	0f be c0             	movsbl %al,%eax
  100acd:	8d 14 50             	lea    (%rax,%rdx,2),%edx
  100ad0:	0f b6 01             	movzbl (%rcx),%eax
  100ad3:	8d 70 d0             	lea    -0x30(%rax),%esi
  100ad6:	40 80 fe 09          	cmp    $0x9,%sil
  100ada:	76 e4                	jbe    100ac0 <cli+0x3f0>
  100adc:	88 15 e0 0c 00 00    	mov    %dl,0xce0(%rip)        # 1017c2 <t_pos_y>
  100ae2:	e9 a9 fc ff ff       	jmp    100790 <cli+0xc0>
  100ae7:	31 d2                	xor    %edx,%edx
  100ae9:	eb f1                	jmp    100adc <cli+0x40c>
  100aeb:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

0000000000100af0 <neofetch>:
  100af0:	55                   	push   %rbp
  100af1:	bf b8 12 10 00       	mov    $0x1012b8,%edi
  100af6:	c7 05 c4 0c 00 00 52 	movl   $0x32a852,0xcc4(%rip)        # 1017c4 <t_color_char>
  100afd:	a8 32 00 
  100b00:	48 89 e5             	mov    %rsp,%rbp
  100b03:	e8 b8 f7 ff ff       	call   1002c0 <print>
  100b08:	5d                   	pop    %rbp
  100b09:	c7 05 b1 0c 00 00 ff 	movl   $0xffffff,0xcb1(%rip)        # 1017c4 <t_color_char>
  100b10:	ff ff 00 
  100b13:	c3                   	ret
  100b14:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  100b1b:	00 00 00 00 
  100b1f:	90                   	nop

0000000000100b20 <strcmp>:
  100b20:	eb 2a                	jmp    100b4c <strcmp+0x2c>
  100b22:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  100b29:	00 00 00 00 
  100b2d:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  100b34:	00 00 00 00 
  100b38:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  100b3f:	00 
  100b40:	38 06                	cmp    %al,(%rsi)
  100b42:	75 11                	jne    100b55 <strcmp+0x35>
  100b44:	48 83 c7 01          	add    $0x1,%rdi
  100b48:	48 83 c6 01          	add    $0x1,%rsi
  100b4c:	0f b6 07             	movzbl (%rdi),%eax
  100b4f:	84 c0                	test   %al,%al
  100b51:	75 ed                	jne    100b40 <strcmp+0x20>
  100b53:	31 c0                	xor    %eax,%eax
  100b55:	0f b6 16             	movzbl (%rsi),%edx
  100b58:	29 d0                	sub    %edx,%eax
  100b5a:	c3                   	ret
  100b5b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

0000000000100b60 <strlen>:
  100b60:	31 c0                	xor    %eax,%eax
  100b62:	80 3f 00             	cmpb   $0x0,(%rdi)
  100b65:	74 19                	je     100b80 <strlen+0x20>
  100b67:	66 0f 1f 84 00 00 00 	nopw   0x0(%rax,%rax,1)
  100b6e:	00 00 
  100b70:	83 c0 01             	add    $0x1,%eax
  100b73:	0f b6 d0             	movzbl %al,%edx
  100b76:	80 3c 17 00          	cmpb   $0x0,(%rdi,%rdx,1)
  100b7a:	75 f4                	jne    100b70 <strlen+0x10>
  100b7c:	c3                   	ret
  100b7d:	0f 1f 00             	nopl   (%rax)
  100b80:	c3                   	ret
  100b81:	66 2e 0f 1f 84 00 00 	cs nopw 0x0(%rax,%rax,1)
  100b88:	00 00 00 
  100b8b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

0000000000100b90 <get_glyph>:
  100b90:	48 0f be c7          	movsbq %dil,%rax
  100b94:	31 d2                	xor    %edx,%edx
  100b96:	40 84 ff             	test   %dil,%dil
  100b99:	48 8d 04 c5 a0 0e 10 	lea    0x100ea0(,%rax,8),%rax
  100ba0:	00 
  100ba1:	48 0f 48 c2          	cmovs  %rdx,%rax
  100ba5:	c3                   	ret
  100ba6:	66 2e 0f 1f 84 00 00 	cs nopw 0x0(%rax,%rax,1)
  100bad:	00 00 00 

0000000000100bb0 <draw_pixel_array>:
  100bb0:	55                   	push   %rbp
  100bb1:	49 89 d2             	mov    %rdx,%r10
  100bb4:	48 89 e5             	mov    %rsp,%rbp
  100bb7:	41 57                	push   %r15
  100bb9:	49 89 ff             	mov    %rdi,%r15
  100bbc:	41 56                	push   %r14
  100bbe:	45 8d 70 08          	lea    0x8(%r8),%r14d
  100bc2:	41 55                	push   %r13
  100bc4:	41 89 cd             	mov    %ecx,%r13d
  100bc7:	41 54                	push   %r12
  100bc9:	45 89 c4             	mov    %r8d,%r12d
  100bcc:	53                   	push   %rbx
  100bcd:	44 0f af e6          	imul   %esi,%r12d
  100bd1:	89 f3                	mov    %esi,%ebx
  100bd3:	48 83 ec 10          	sub    $0x10,%rsp
  100bd7:	44 89 4d cc          	mov    %r9d,-0x34(%rbp)
  100bdb:	41 01 cc             	add    %ecx,%r12d
  100bde:	66 90                	xchg   %ax,%ax
  100be0:	45 89 c3             	mov    %r8d,%r11d
  100be3:	44 89 65 d4          	mov    %r12d,-0x2c(%rbp)
  100be7:	41 0f b6 3f          	movzbl (%r15),%edi
  100beb:	44 89 e8             	mov    %r13d,%eax
  100bee:	41 f7 d3             	not    %r11d
  100bf1:	89 5d d0             	mov    %ebx,-0x30(%rbp)
  100bf4:	44 89 e2             	mov    %r12d,%edx
  100bf7:	31 c9                	xor    %ecx,%ecx
  100bf9:	41 c1 eb 1f          	shr    $0x1f,%r11d
  100bfd:	eb 2f                	jmp    100c2e <draw_pixel_array+0x7e>
  100bff:	90                   	nop
  100c00:	85 c0                	test   %eax,%eax
  100c02:	78 1c                	js     100c20 <draw_pixel_array+0x70>
  100c04:	41 3b 42 08          	cmp    0x8(%r10),%eax
  100c08:	73 16                	jae    100c20 <draw_pixel_array+0x70>
  100c0a:	45 84 db             	test   %r11b,%r11b
  100c0d:	0f 85 8d 00 00 00    	jne    100ca0 <draw_pixel_array+0xf0>
  100c13:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  100c1a:	00 00 00 00 
  100c1e:	66 90                	xchg   %ax,%ax
  100c20:	83 c1 01             	add    $0x1,%ecx
  100c23:	83 c2 01             	add    $0x1,%edx
  100c26:	83 c0 01             	add    $0x1,%eax
  100c29:	83 f9 08             	cmp    $0x8,%ecx
  100c2c:	74 42                	je     100c70 <draw_pixel_array+0xc0>
  100c2e:	be 80 00 00 00       	mov    $0x80,%esi
  100c33:	d3 fe                	sar    %cl,%esi
  100c35:	85 fe                	test   %edi,%esi
  100c37:	75 c7                	jne    100c00 <draw_pixel_array+0x50>
  100c39:	85 c0                	test   %eax,%eax
  100c3b:	78 e3                	js     100c20 <draw_pixel_array+0x70>
  100c3d:	41 3b 42 08          	cmp    0x8(%r10),%eax
  100c41:	73 dd                	jae    100c20 <draw_pixel_array+0x70>
  100c43:	45 84 db             	test   %r11b,%r11b
  100c46:	74 d8                	je     100c20 <draw_pixel_array+0x70>
  100c48:	45 3b 42 0c          	cmp    0xc(%r10),%r8d
  100c4c:	73 d2                	jae    100c20 <draw_pixel_array+0x70>
  100c4e:	49 8b 1a             	mov    (%r10),%rbx
  100c51:	48 63 f2             	movslq %edx,%rsi
  100c54:	83 c1 01             	add    $0x1,%ecx
  100c57:	83 c2 01             	add    $0x1,%edx
  100c5a:	83 c0 01             	add    $0x1,%eax
  100c5d:	c7 04 b3 00 00 00 00 	movl   $0x0,(%rbx,%rsi,4)
  100c64:	83 f9 08             	cmp    $0x8,%ecx
  100c67:	75 c5                	jne    100c2e <draw_pixel_array+0x7e>
  100c69:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
  100c70:	44 8b 65 d4          	mov    -0x2c(%rbp),%r12d
  100c74:	8b 5d d0             	mov    -0x30(%rbp),%ebx
  100c77:	41 83 c0 01          	add    $0x1,%r8d
  100c7b:	49 83 c7 01          	add    $0x1,%r15
  100c7f:	41 01 dc             	add    %ebx,%r12d
  100c82:	45 39 f0             	cmp    %r14d,%r8d
  100c85:	0f 85 55 ff ff ff    	jne    100be0 <draw_pixel_array+0x30>
  100c8b:	48 83 c4 10          	add    $0x10,%rsp
  100c8f:	5b                   	pop    %rbx
  100c90:	41 5c                	pop    %r12
  100c92:	41 5d                	pop    %r13
  100c94:	41 5e                	pop    %r14
  100c96:	41 5f                	pop    %r15
  100c98:	5d                   	pop    %rbp
  100c99:	c3                   	ret
  100c9a:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
  100ca0:	45 3b 42 0c          	cmp    0xc(%r10),%r8d
  100ca4:	0f 83 76 ff ff ff    	jae    100c20 <draw_pixel_array+0x70>
  100caa:	49 8b 1a             	mov    (%r10),%rbx
  100cad:	44 8b 65 cc          	mov    -0x34(%rbp),%r12d
  100cb1:	48 63 f2             	movslq %edx,%rsi
  100cb4:	44 89 24 b3          	mov    %r12d,(%rbx,%rsi,4)
  100cb8:	e9 63 ff ff ff       	jmp    100c20 <draw_pixel_array+0x70>
  100cbd:	0f 1f 00             	nopl   (%rax)

0000000000100cc0 <draw_char>:
  100cc0:	55                   	push   %rbp
  100cc1:	48 89 f8             	mov    %rdi,%rax
  100cc4:	48 0f be fe          	movsbq %sil,%rdi
  100cc8:	48 8d 3c fd a0 0e 10 	lea    0x100ea0(,%rdi,8),%rdi
  100ccf:	00 
  100cd0:	48 89 e5             	mov    %rsp,%rbp
  100cd3:	48 83 ec 50          	sub    $0x50,%rsp
  100cd7:	40 84 f6             	test   %sil,%sil
  100cda:	be 00 00 00 00       	mov    $0x0,%esi
  100cdf:	4c 89 4d f8          	mov    %r9,-0x8(%rbp)
  100ce3:	48 0f 48 fe          	cmovs  %rsi,%rdi
  100ce7:	8b 70 10             	mov    0x10(%rax),%esi
  100cea:	4c 8d 55 10          	lea    0x10(%rbp),%r10
  100cee:	44 0f b6 4d f8       	movzbl -0x8(%rbp),%r9d
  100cf3:	4c 8d 5d d0          	lea    -0x30(%rbp),%r11
  100cf7:	c7 45 b8 28 00 00 00 	movl   $0x28,-0x48(%rbp)
  100cfe:	c1 ee 02             	shr    $0x2,%esi
  100d01:	4c 89 55 c0          	mov    %r10,-0x40(%rbp)
  100d05:	41 51                	push   %r9
  100d07:	45 89 c1             	mov    %r8d,%r9d
  100d0a:	41 89 c8             	mov    %ecx,%r8d
  100d0d:	89 d1                	mov    %edx,%ecx
  100d0f:	48 89 c2             	mov    %rax,%rdx
  100d12:	4c 89 5d c8          	mov    %r11,-0x38(%rbp)
  100d16:	e8 95 fe ff ff       	call   100bb0 <draw_pixel_array>
  100d1b:	58                   	pop    %rax
  100d1c:	c9                   	leave
  100d1d:	c3                   	ret
  100d1e:	66 90                	xchg   %ax,%ax

0000000000100d20 <print_text>:
  100d20:	55                   	push   %rbp
  100d21:	48 89 e5             	mov    %rsp,%rbp
  100d24:	41 57                	push   %r15
  100d26:	41 56                	push   %r14
  100d28:	49 89 f6             	mov    %rsi,%r14
  100d2b:	41 55                	push   %r13
  100d2d:	41 54                	push   %r12
  100d2f:	53                   	push   %rbx
  100d30:	0f be 36             	movsbl (%rsi),%esi
  100d33:	40 84 f6             	test   %sil,%sil
  100d36:	74 3c                	je     100d74 <print_text+0x54>
  100d38:	49 89 fd             	mov    %rdi,%r13
  100d3b:	41 89 d7             	mov    %edx,%r15d
  100d3e:	41 89 cc             	mov    %ecx,%r12d
  100d41:	44 89 c3             	mov    %r8d,%ebx
  100d44:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  100d4b:	00 00 00 00 
  100d4f:	90                   	nop
  100d50:	44 89 fa             	mov    %r15d,%edx
  100d53:	41 89 d8             	mov    %ebx,%r8d
  100d56:	44 89 e1             	mov    %r12d,%ecx
  100d59:	4c 89 ef             	mov    %r13,%rdi
  100d5c:	31 c0                	xor    %eax,%eax
  100d5e:	49 83 c6 01          	add    $0x1,%r14
  100d62:	41 83 c7 08          	add    $0x8,%r15d
  100d66:	e8 55 ff ff ff       	call   100cc0 <draw_char>
  100d6b:	41 0f be 36          	movsbl (%r14),%esi
  100d6f:	40 84 f6             	test   %sil,%sil
  100d72:	75 dc                	jne    100d50 <print_text+0x30>
  100d74:	5b                   	pop    %rbx
  100d75:	41 5c                	pop    %r12
  100d77:	41 5d                	pop    %r13
  100d79:	41 5e                	pop    %r14
  100d7b:	41 5f                	pop    %r15
  100d7d:	5d                   	pop    %rbp
  100d7e:	c3                   	ret
  100d7f:	90                   	nop

0000000000100d80 <heap_init>:
  100d80:	48 89 3d 99 4c 00 00 	mov    %rdi,0x4c99(%rip)        # 105a20 <heap_ptr>
  100d87:	48 01 f7             	add    %rsi,%rdi
  100d8a:	48 89 3d 87 4c 00 00 	mov    %rdi,0x4c87(%rip)        # 105a18 <heap_end>
  100d91:	c3                   	ret
  100d92:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  100d99:	00 00 00 00 
  100d9d:	0f 1f 00             	nopl   (%rax)

0000000000100da0 <kmalloc>:
  100da0:	48 8b 05 79 4c 00 00 	mov    0x4c79(%rip),%rax        # 105a20 <heap_ptr>
  100da7:	48 01 c7             	add    %rax,%rdi
  100daa:	48 39 3d 67 4c 00 00 	cmp    %rdi,0x4c67(%rip)        # 105a18 <heap_end>
  100db1:	72 0d                	jb     100dc0 <kmalloc+0x20>
  100db3:	48 89 3d 66 4c 00 00 	mov    %rdi,0x4c66(%rip)        # 105a20 <heap_ptr>
  100dba:	c3                   	ret
  100dbb:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  100dc0:	31 c0                	xor    %eax,%eax
  100dc2:	c3                   	ret
  100dc3:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  100dca:	00 00 00 00 
  100dce:	66 90                	xchg   %ax,%ax

0000000000100dd0 <kmalloc_aligned>:
  100dd0:	48 8b 05 49 4c 00 00 	mov    0x4c49(%rip),%rax        # 105a20 <heap_ptr>
  100dd7:	48 8d 14 38          	lea    (%rax,%rdi,1),%rdx
  100ddb:	48 39 15 36 4c 00 00 	cmp    %rdx,0x4c36(%rip)        # 105a18 <heap_end>
  100de2:	72 1c                	jb     100e00 <kmalloc_aligned+0x30>
  100de4:	48 8d 44 30 ff       	lea    -0x1(%rax,%rsi,1),%rax
  100de9:	48 f7 de             	neg    %rsi
  100dec:	48 21 f0             	and    %rsi,%rax
  100def:	48 01 c7             	add    %rax,%rdi
  100df2:	48 89 3d 27 4c 00 00 	mov    %rdi,0x4c27(%rip)        # 105a20 <heap_ptr>
  100df9:	c3                   	ret
  100dfa:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
  100e00:	31 c0                	xor    %eax,%eax
  100e02:	c3                   	ret
  100e03:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
  100e0a:	00 00 00 00 
  100e0e:	66 90                	xchg   %ax,%ax

0000000000100e10 <kfree>:
  100e10:	c3                   	ret
