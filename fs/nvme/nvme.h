// #pragma once
// #include <stdint.h>
// #include <stdbool.h>
// #include <stddef.h>

// #define NVME_CLASS_CODE 0x01
// #define NVME_SUBCLASS_CODE 0x08

// #define NVME_REG_CAP 0x0000    // Controller Capabilities
// #define NVME_REG_VER 0x0008    // Version
// #define NVME_REG_INTMS 0x000C  // Interrupt Mask Set
// #define NVME_REG_INTMC 0x0010  // Interrupt Mask Clear
// #define NVME_REG_CC 0x0014     // Controller Configuration
// #define NVME_REG_CSTS 0x001C   // Controller Status
// #define NVME_REG_AQA 0x0024    // Admin Queue Attributes
// #define NVME_REG_ASQ 0x0028    // Admin Submission Queue Base Addr
// #define NVME_REG_ACQ 0x0030    // Admin Completion Queue Base Addr
// #define NVME_REG_SQTDBL 0x1000 // Submission Queue Tail Doorbell (per queue)
// #define NVME_REG_CQHDBL 0x1000 // Completion Queue Head Doorbell (per queue)

// #define NVME_ADMIN_DELETE_SQ 0x00
// #define NVME_ADMIN_CREATE_SQ 0x01
// #define NVME_ADMIN_GET_LOG 0x02
// #define NVME_ADMIN_DELETE_CQ 0x04
// #define NVME_ADMIN_CREATE_CQ 0x05
// #define NVME_ADMIN_IDENTIFY 0x06
// #define NVME_ADMIN_ABORT 0x08
// #define NVME_ADMIN_SET_FEATURES 0x09
// #define NVME_ADMIN_GET_FEATURES 0x0A

// #define NVME_CMD_FLUSH 0x00
// #define NVME_CMD_WRITE 0x01
// #define NVME_CMD_READ 0x02

// // Completion Queue Entry Status Codes
// #define NVME_SC_SUCCESS 0x0
// #define NVME_SC_INVALID_OPCODE 0x1
// #define NVME_SC_INVALID_FIELD 0x2
// #define NVME_SC_COMMAND_ID_CONFLICT 0x3
// #define NVME_SC_DATA_TRANSFER_ERROR 0x4
// #define NVME_SC_ABORTED_POWER_LOSS 0x5
// #define NVME_SC_INTERNAL_DEVICE_ERROR 0x6
// #define NVME_SC_ABORT_REQUESTED 0x7
// #define NVME_SC_ABORT_SQ_DELETION 0x8
// #define NVME_SC_ABORT_FAILED_FUSED 0x9
// #define NVME_SC_ABORT_MISSING_FUSED 0xA
// #define NVME_SC_INVALID_NAMESPACE 0xB
// #define NVME_SC_CMD_SEQ_ERROR 0xC
// // ... (можна дописати ще, але для старту цього вистачає)

// typedef struct __attribute__((packed))
// {
// 	uint8_t opcode;
// 	uint8_t flags;
// 	uint16_t command_id;
// 	uint32_t nsid;
// 	uint64_t rsvd2;
// 	uint64_t mptr;
// 	uint64_t prp1;
// 	uint64_t prp2;
// 	uint32_t command_specific[5];
// } nvme_sq_entry;

// typedef struct __attribute__((packed))
// {
// 	uint32_t result; // DW0
// 	uint32_t rsvd;
// 	uint16_t sq_head;
// 	uint16_t sq_id;
// 	uint16_t command_id;
// 	uint16_t status; // includes Phase Tag
// } nvme_cq_entry;

// // NVMe Command (SQ entry)
// typedef struct
// {
// 	uint8_t opc;	  // Opcode
// 	uint8_t fuse : 2; // Fused operation
// 	uint8_t rsvd1 : 5;
// 	uint8_t psdt : 1; // PRP or SGL
// 	uint16_t cid;	  // Command ID

// 	uint32_t nsid; // Namespace ID
// 	uint64_t rsvd2;
// 	uint64_t mptr; // Metadata pointer
// 	uint64_t prp1; // PRP entry 1
// 	uint64_t prp2; // PRP entry 2

// 	union
// 	{
// 		struct
// 		{
// 			uint64_t slba; // Starting LBA
// 			uint16_t nlb;  // Number of logical blocks
// 			uint16_t control;
// 			uint32_t dsmgmt;
// 			uint32_t reftag;
// 			uint16_t apptag;
// 			uint16_t appmask;
// 		} read;
// 	} d;
// } __attribute__((packed)) nvme_command_entry;

// // NVMe Completion (CQ entry)
// typedef struct
// {
// 	uint32_t dw0; // Command-specific
// 	uint32_t rsvd;
// 	uint16_t sq_head; // SQ head pointer
// 	uint16_t sq_id;	  // Submission Queue ID
// 	uint16_t cid;	  // Command ID
// 	uint16_t status;  // Status Field (includes Phase Tag)
// } __attribute__((packed)) nvme_completion;

// struct nvme_controller
// {
// 	volatile uint8_t *bar;
// 	struct nvme_sq_entry *admin_sq;
// 	struct nvme_cq_entry *admin_cq;
// 	uint16_t admin_sq_tail;
// 	uint16_t admin_cq_head;
// 	uint8_t phase;
// };

// int vfs_nvme_read(void *device, uint32_t lba, void *buffer);
// int vfs_nvme_write(void *device, uint32_t lba, const void *buffer);
// void *alloc_page_aligned(void);
// void *alloc_pages_aligned(size_t n);
// int find_nvme_controller(struct nvme_controller *ctrl);
// void nvme_init(struct nvme_controller *ctrl);
// struct nvme_controller *nvme_get();
// void nvme_admin_identify(struct nvme_controller *ctrl, void *buffer);
