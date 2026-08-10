// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>

static void total_mapping_size_test(struct kunit *test)
{
	struct elf_phdr empty[] = {
		{ .p_type = PT_LOAD, .p_vaddr = 0, .p_memsz = 0, },
		{ .p_type = PT_INTERP, .p_vaddr = 10, .p_memsz = 999999, },
	};
	/*
	 * readelf -lW /bin/mount | grep '^  .*0x0' | awk '{print "\t\t{ .p_type = PT_" \
	 *				$1 ", .p_vaddr = " $3 ", .p_memsz = " $6 ", },"}'
	 */
	struct elf_phdr mount[] = {
		{ .p_type = PT_PHDR, .p_vaddr = 0x00000040, .p_memsz = 0x0002d8, },
		{ .p_type = PT_INTERP, .p_vaddr = 0x00000318, .p_memsz = 0x00001c, },
		{ .p_type = PT_LOAD, .p_vaddr = 0x00000000, .p_memsz = 0x0033a8, },
		{ .p_type = PT_LOAD, .p_vaddr = 0x00004000, .p_memsz = 0x005c91, },
		{ .p_type = PT_LOAD, .p_vaddr = 0x0000a000, .p_memsz = 0x0022f8, },
		{ .p_type = PT_LOAD, .p_vaddr = 0x0000d330, .p_memsz = 0x000d40, },
		{ .p_type = PT_DYNAMIC, .p_vaddr = 0x0000d928, .p_memsz = 0x000200, },
		{ .p_type = PT_NOTE, .p_vaddr = 0x00000338, .p_memsz = 0x000030, },
		{ .p_type = PT_NOTE, .p_vaddr = 0x00000368, .p_memsz = 0x000044, },
		{ .p_type = PT_GNU_PROPERTY, .p_vaddr = 0x00000338, .p_memsz = 0x000030, },
		{ .p_type = PT_GNU_EH_FRAME, .p_vaddr = 0x0000b490, .p_memsz = 0x0001ec, },
		{ .p_type = PT_GNU_STACK, .p_vaddr = 0x00000000, .p_memsz = 0x000000, },
		{ .p_type = PT_GNU_RELRO, .p_vaddr = 0x0000d330, .p_memsz = 0x000cd0, },
	};
	size_t mount_size = 0xE070;
	/* https://lore.kernel.org/linux-fsdevel/YfF18Dy85mCntXrx@fractal.localdomain */
	struct elf_phdr unordered[] = {
		{ .p_type = PT_LOAD, .p_vaddr = 0x00000000, .p_memsz = 0x0033a8, },
		{ .p_type = PT_LOAD, .p_vaddr = 0x0000d330, .p_memsz = 0x000d40, },
		{ .p_type = PT_LOAD, .p_vaddr = 0x00004000, .p_memsz = 0x005c91, },
		{ .p_type = PT_LOAD, .p_vaddr = 0x0000a000, .p_memsz = 0x0022f8, },
	};

	/* No headers, no size. */
	KUNIT_EXPECT_EQ(test, total_mapping_size(NULL, 0), 0);
	KUNIT_EXPECT_EQ(test, total_mapping_size(empty, 0), 0);
	/* Empty headers, no size. */
	KUNIT_EXPECT_EQ(test, total_mapping_size(empty, 1), 0);
	/* No PT_LOAD headers, no size. */
	KUNIT_EXPECT_EQ(test, total_mapping_size(&empty[1], 1), 0);
	/* Empty PT_LOAD and non-PT_LOAD headers, no size. */
	KUNIT_EXPECT_EQ(test, total_mapping_size(empty, 2), 0);

	/* Normal set of PT_LOADS, and expected size. */
	KUNIT_EXPECT_EQ(test, total_mapping_size(mount, ARRAY_SIZE(mount)), mount_size);
	/* Unordered PT_LOADs result in same size. */
	KUNIT_EXPECT_EQ(test, total_mapping_size(unordered, ARRAY_SIZE(unordered)), mount_size);
}

#ifdef CONFIG_ARCH_LINX
static size_t append_pto_note(u8 *buffer, size_t off, const char *desc,
			      size_t descsz, const char name[4], u32 type)
{
	struct elf_note note = {
		.n_namesz = PTO_ISA_IDENTITY_NOTE_NAMESZ,
		.n_descsz = descsz,
		.n_type = type,
	};
	size_t namesz = ALIGN(note.n_namesz, 4);
	size_t padded_descsz = ALIGN(descsz, 4);

	memcpy(buffer + off, &note, sizeof(note));
	off += sizeof(note);
	memcpy(buffer + off, name, note.n_namesz);
	memset(buffer + off + note.n_namesz, 0, namesz - note.n_namesz);
	off += namesz;
	memcpy(buffer + off, desc, descsz);
	memset(buffer + off + descsz, 0, padded_descsz - descsz);

	return off + padded_descsz;
}

static void linx_pto_identity_note_valid_test(struct kunit *test)
{
	u8 note[512] = {};
	bool found = false;
	size_t size;

	size = append_pto_note(note, 0, linx_pto_isa_identity,
			       sizeof(linx_pto_isa_identity) - 1, "PTO\0",
			       PTO_NT_ISA_IDENTITY);
	KUNIT_EXPECT_EQ(test, linx_pto_isa_note_parse(note, size, &found), 0);
	KUNIT_EXPECT_TRUE(test, found);
	KUNIT_EXPECT_EQ(test, linx_pto_isa_identity_status(found), 0);
}

static void linx_pto_identity_exact_v058_test(struct kunit *test)
{
	static const char expected_hash[] =
		"0cad2272ada8f53fc8354e22568099fe8d6bd4b7832c837260cd370b0fc76ffa";
	const char *abi;

	abi = strstr(linx_pto_isa_identity, "pto-isa-0.58.0-mode-function-v1");
	KUNIT_EXPECT_EQ(test, sizeof(linx_pto_isa_identity) - 1, (size_t)165);
	KUNIT_EXPECT_NOT_NULL(test, abi);
	KUNIT_EXPECT_NOT_NULL(test, strstr(linx_pto_isa_identity, expected_hash));
	KUNIT_EXPECT_NOT_NULL(test, strstr(linx_pto_isa_identity, "0.58.0"));
}

static void linx_pto_identity_note_missing_test(struct kunit *test)
{
	u8 note[512] = {};
	bool found = false;
	size_t size;

	size = append_pto_note(note, 0, linx_pto_isa_identity,
			       sizeof(linx_pto_isa_identity) - 1, "GNU\0",
			       PTO_NT_ISA_IDENTITY);
	KUNIT_EXPECT_EQ(test, linx_pto_isa_note_parse(note, size, &found), 0);
	KUNIT_EXPECT_FALSE(test, found);
	KUNIT_EXPECT_EQ(test, linx_pto_isa_identity_status(found), -ENOEXEC);
}

static void linx_pto_identity_note_malformed_test(struct kunit *test)
{
	u8 note[512] = {};
	bool found = false;
	size_t size;

	size = append_pto_note(note, 0, linx_pto_isa_identity,
			       sizeof(linx_pto_isa_identity) - 1, "PTO\0",
			       PTO_NT_ISA_IDENTITY);
	KUNIT_EXPECT_EQ(test,
			linx_pto_isa_note_parse(note, size - 1, &found),
			-ENOEXEC);
}

static void linx_pto_identity_note_oversized_test(struct kunit *test)
{
	u8 note[1] = {};
	bool found = false;
	int ret;

	ret = linx_pto_isa_note_parse(note, LINX_PTO_ISA_NOTE_SCAN_MAX + 1,
				      &found);
	KUNIT_EXPECT_EQ(test, ret, -ENOEXEC);
}

static void linx_pto_identity_legacy_machine_rejected_test(struct kunit *test)
{
	struct elfhdr active = { .e_machine = EM_LINXISA };
	struct elfhdr legacy = { .e_machine = EM_LINX_V5 };

	KUNIT_EXPECT_TRUE(test, elf_check_arch(&active));
	KUNIT_EXPECT_FALSE(test, elf_check_arch(&legacy));
}

static void expect_pto_identity_mismatch(struct kunit *test, char *desc)
{
	u8 note[512] = {};
	bool found = false;
	size_t size;

	size = append_pto_note(note, 0, desc,
			       sizeof(linx_pto_isa_identity) - 1, "PTO\0",
			       PTO_NT_ISA_IDENTITY);
	KUNIT_EXPECT_EQ(test,
			linx_pto_isa_note_parse(note, size, &found),
			-ENOEXEC);
	KUNIT_EXPECT_FALSE(test, found);
}

static void linx_pto_identity_note_release_mismatch_test(struct kunit *test)
{
	char desc[sizeof(linx_pto_isa_identity)];
	char *release;

	memcpy(desc, linx_pto_isa_identity, sizeof(desc));
	release = strstr(desc, "0.58.0");
	KUNIT_ASSERT_NOT_NULL(test, release);
	release[5] = '2';
	expect_pto_identity_mismatch(test, desc);
}

static void linx_pto_identity_note_abi_mismatch_test(struct kunit *test)
{
	char desc[sizeof(linx_pto_isa_identity)];
	char *abi;

	memcpy(desc, linx_pto_isa_identity, sizeof(desc));
	abi = strstr(desc, "mode-function-v1");
	KUNIT_ASSERT_NOT_NULL(test, abi);
	abi[15] = '2';
	expect_pto_identity_mismatch(test, desc);
}

static void linx_pto_identity_note_hash_mismatch_test(struct kunit *test)
{
	char desc[sizeof(linx_pto_isa_identity)];
	char *hash;

	memcpy(desc, linx_pto_isa_identity, sizeof(desc));
	hash = strstr(desc, "0cad2272ada8f53f");
	KUNIT_ASSERT_NOT_NULL(test, hash);
	hash[0] = 'f';
	expect_pto_identity_mismatch(test, desc);
}

static void linx_pto_identity_note_trailing_nul_test(struct kunit *test)
{
	u8 note[512] = {};
	bool found = false;
	size_t size;

	size = append_pto_note(note, 0, linx_pto_isa_identity,
			       sizeof(linx_pto_isa_identity), "PTO\0",
			       PTO_NT_ISA_IDENTITY);
	KUNIT_EXPECT_EQ(test,
			linx_pto_isa_note_parse(note, size, &found),
			-ENOEXEC);
}

static void linx_pto_identity_note_duplicate_test(struct kunit *test)
{
	u8 notes[1024] = {};
	char mismatch[sizeof(linx_pto_isa_identity)];
	bool found = false;
	size_t size;

	size = append_pto_note(notes, 0, linx_pto_isa_identity,
			       sizeof(linx_pto_isa_identity) - 1, "PTO\0",
			       PTO_NT_ISA_IDENTITY);
	size = append_pto_note(notes, size, linx_pto_isa_identity,
			       sizeof(linx_pto_isa_identity) - 1, "PTO\0",
			       PTO_NT_ISA_IDENTITY);
	KUNIT_EXPECT_EQ(test, linx_pto_isa_note_parse(notes, size, &found), 0);
	KUNIT_EXPECT_TRUE(test, found);

	memcpy(mismatch, linx_pto_isa_identity, sizeof(mismatch));
	strstr(mismatch, "0.58.0")[5] = '1';
	size = append_pto_note(notes, 0, linx_pto_isa_identity,
			       sizeof(linx_pto_isa_identity) - 1, "PTO\0",
			       PTO_NT_ISA_IDENTITY);
	size = append_pto_note(notes, size, mismatch, sizeof(mismatch) - 1,
			       "PTO\0", PTO_NT_ISA_IDENTITY);
	found = false;
	KUNIT_EXPECT_EQ(test,
			linx_pto_isa_note_parse(notes, size, &found),
			-ENOEXEC);
}
#endif

static struct kunit_case binfmt_elf_test_cases[] = {
	KUNIT_CASE(total_mapping_size_test),
#ifdef CONFIG_ARCH_LINX
	KUNIT_CASE(linx_pto_identity_note_valid_test),
	KUNIT_CASE(linx_pto_identity_exact_v058_test),
	KUNIT_CASE(linx_pto_identity_note_missing_test),
	KUNIT_CASE(linx_pto_identity_note_malformed_test),
	KUNIT_CASE(linx_pto_identity_note_oversized_test),
	KUNIT_CASE(linx_pto_identity_legacy_machine_rejected_test),
	KUNIT_CASE(linx_pto_identity_note_release_mismatch_test),
	KUNIT_CASE(linx_pto_identity_note_abi_mismatch_test),
	KUNIT_CASE(linx_pto_identity_note_hash_mismatch_test),
	KUNIT_CASE(linx_pto_identity_note_trailing_nul_test),
	KUNIT_CASE(linx_pto_identity_note_duplicate_test),
#endif
	{},
};

static struct kunit_suite binfmt_elf_test_suite = {
	.name = KBUILD_MODNAME,
	.test_cases = binfmt_elf_test_cases,
};

kunit_test_suite(binfmt_elf_test_suite);
