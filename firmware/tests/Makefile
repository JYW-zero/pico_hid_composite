# ========================================
#  Linux/Mac 测试 Makefile（支持代码覆盖率）
# ========================================

CC = gcc
UNITY_DEFS = -DUNITY_INT_WIDTH=32 -DUNITY_LONG_WIDTH=32 -DUNITY_POINTER_WIDTH=32
CFLAGS = -I../include -Iunity/src -Imock $(UNITY_DEFS) -Wno-error -std=gnu11
# 覆盖率编译选项
COVERAGE_CFLAGS = --coverage -O0 -g
LDFLAGS =
COVERAGE_LDFLAGS = --coverage

SRCS = unity/src/unity.c \
       mock/mock_gpio.c \
       mock/mock_time.c \
       mock/mock_flash.c \
       mock/mock_tusb.c \
       mock/mock_watchdog.c \
       ../src/board/config.c \
       ../src/middleware/debounce.c \
       ../src/middleware/fault.c \
       ../src/middleware/scheduler.c \
       ../src/middleware/crc32.c \
       ../src/app/keymap.c \
       ../src/app/macro.c \
       ../src/device/encoder.c \
       ../src/protocol/config_validator.c \
       ../src/protocol/config_block_receiver.c \
       ../src/protocol/config_hid_helpers.c \
       ../src/protocol/config_lock.c \
       ../src/protocol/command_confirm.c \
       ../src/protocol/capability.c \
       ../src/protocol/command_gate.c \
       test_debounce.c \
       test_keymap.c \
       test_encoder.c \
       test_scheduler.c \
       test_config.c \
       test_crc.c \
       test_macro.c \
       test_fault.c \
       test_config_validator.c \
       test_config_block_receiver.c \
       test_config_hid_helpers.c \
       test_config_lock.c \
       test_command_confirm.c \
       test_capability.c \
       test_command_gate.c \
       test_runner.c
OUTPUT = run_tests

# 覆盖率相关
COVERAGE_DIR = coverage
GCOV = gcov
LCOV = lcov
GENHTML = genhtml

# 覆盖率阈值（百分比）
COVERAGE_THRESHOLD = 60

.PHONY: all clean test coverage coverage-report coverage-check

all: test

# 普通构建（无覆盖率）
$(OUTPUT): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(OUTPUT) $(LDFLAGS)

# 带覆盖率的构建
$(OUTPUT)_coverage: $(SRCS)
	$(CC) $(CFLAGS) $(COVERAGE_CFLAGS) $(SRCS) -o $(OUTPUT)_coverage $(COVERAGE_LDFLAGS)

test: $(OUTPUT)
	./$(OUTPUT)

# 运行带覆盖率的测试
coverage: $(OUTPUT)_coverage
	@echo "=========================================="
	@echo "  运行带覆盖率的测试"
	@echo "=========================================="
	./$(OUTPUT)_coverage
	@echo ""
	@echo "测试完成，正在生成覆盖率数据..."

# 生成覆盖率报告
coverage-report: coverage
	@echo "=========================================="
	@echo "  生成覆盖率报告"
	@echo "=========================================="
	@mkdir -p $(COVERAGE_DIR)

	# 使用 gcov 生成 .gcov 文件
	$(GCOV) -b -c -o . $(SRCS) 2>/dev/null || true

	# 移动 .gcov 文件到覆盖率目录
	mv -f *.gcov $(COVERAGE_DIR)/ 2>/dev/null || true

	# 如果安装了 lcov，生成更详细的报告
	@if command -v $(LCOV) >/dev/null 2>&1; then \
		echo "使用 lcov 生成详细报告..."; \
		$(LCOV) --capture --directory . --output-file $(COVERAGE_DIR)/coverage.info --rc lcov_branch_coverage=1 2>/dev/null || true; \
		$(LCOV) --remove $(COVERAGE_DIR)/coverage.info '*/unity/*' '*/mock/*' '*/test_*' --output-file $(COVERAGE_DIR)/coverage_filtered.info --rc lcov_branch_coverage=1 2>/dev/null || true; \
		if command -v $(GENHTML) >/dev/null 2>&1; then \
			$(GENHTML) $(COVERAGE_DIR)/coverage_filtered.info --output-directory $(COVERAGE_DIR)/html --branch-coverage --title "Firmware Test Coverage" 2>/dev/null || true; \
			echo "HTML 报告已生成: $(COVERAGE_DIR)/html/index.html"; \
		fi; \
	fi

	@echo ""
	@echo "覆盖率报告生成完成！"
	@echo "  - gcov 文件: $(COVERAGE_DIR)/*.gcov"
	@echo "  - lcov 数据: $(COVERAGE_DIR)/coverage_filtered.info"
	@echo "  - HTML 报告: $(COVERAGE_DIR)/html/index.html"

# 覆盖率阈值检查
coverage-check: coverage-report
	@echo "=========================================="
	@echo "  覆盖率阈值检查（阈值: $(COVERAGE_THRESHOLD)%）"
	@echo "=========================================="

	@if [ -f $(COVERAGE_DIR)/coverage_filtered.info ]; then \
		# 从 lcov 报告中提取行覆盖率
		LINE_COVERAGE=$$(grep -A1 "lines" $(COVERAGE_DIR)/coverage_filtered.info | tail -1 | awk -F: '{print $$2}' | awk -F% '{print $$1}'); \
		if [ -z "$$LINE_COVERAGE" ]; then \
			LINE_COVERAGE=$$(lcov --summary $(COVERAGE_DIR)/coverage_filtered.info 2>/dev/null | grep "lines" | awk '{print $$2}' | tr -d '%'); \
		fi; \
		if [ -n "$$LINE_COVERAGE" ]; then \
			echo "行覆盖率: $$LINE_COVERAGE%"; \
			if [ $$(echo "$$LINE_COVERAGE < $(COVERAGE_THRESHOLD)" | bc -l) -eq 1 ]; then \
				echo "❌ 覆盖率 $$LINE_COVERAGE% 低于阈值 $(COVERAGE_THRESHOLD)%"; \
				exit 1; \
			else \
				echo "✅ 覆盖率 $$LINE_COVERAGE% 达到阈值 $(COVERAGE_THRESHOLD)%"; \
			fi; \
		else \
			echo "⚠️  无法提取覆盖率数据，跳过阈值检查"; \
		fi; \
	else \
		echo "⚠️  未找到 lcov 报告，使用 gcov 数据进行检查..."; \
		# 简单统计 gcov 文件中的覆盖率
		TOTAL_LINES=0; \
		COVERED_LINES=0; \
		for f in $(COVERAGE_DIR)/*.gcov; do \
			if [ -f "$$f" ]; then \
				LINES=$$(grep -c "#####" "$$f" 2>/dev/null || echo 0); \
				COVERED=$$(grep -cE "^\s+[0-9]+:" "$$f" 2>/dev/null || echo 0); \
				TOTAL_LINES=$$((TOTAL_LINES + LINES + COVERED)); \
				COVERED_LINES=$$((COVERED_LINES + COVERED)); \
			fi; \
		done; \
		if [ $$TOTAL_LINES -gt 0 ]; then \
			COVERAGE=$$((COVERED_LINES * 100 / TOTAL_LINES)); \
			echo "行覆盖率: $$COVERAGE% (估算)"; \
			if [ $$COVERAGE -lt $(COVERAGE_THRESHOLD) ]; then \
				echo "❌ 覆盖率 $$COVERAGE% 低于阈值 $(COVERAGE_THRESHOLD)%"; \
				exit 1; \
			else \
				echo "✅ 覆盖率 $$COVERAGE% 达到阈值 $(COVERAGE_THRESHOLD)%"; \
			fi; \
		else \
			echo "⚠️  无法计算覆盖率，跳过阈值检查"; \
		fi; \
	fi

# 生成 Cobertura XML 格式（用于 Codecov 等工具）
coverage-cobertura: coverage-report
	@echo "=========================================="
	@echo "  生成 Cobertura XML 格式报告"
	@echo "=========================================="
	@if command -v $(LCOV) >/dev/null 2>&1; then \
		if command -v lcov_cobertura >/dev/null 2>&1; then \
			lcov_cobertura $(COVERAGE_DIR)/coverage_filtered.info -o $(COVERAGE_DIR)/coverage.xml; \
			echo "Cobertura XML 已生成: $(COVERAGE_DIR)/coverage.xml"; \
		else \
			echo "⚠️  lcov_cobertura 未安装，尝试使用 gcovr..."; \
			if command -v gcovr >/dev/null 2>&1; then \
				gcovr --root .. --xml-pretty --output $(COVERAGE_DIR)/coverage.xml --exclude 'unity/' --exclude 'mock/' --exclude 'tests/'; \
				echo "Cobertura XML 已生成: $(COVERAGE_DIR)/coverage.xml"; \
			else \
				echo "❌ 未找到 lcov_cobertura 或 gcovr，无法生成 Cobertura XML"; \
				echo "   安装方法: pip install lcov-cobertura gcovr"; \
				exit 1; \
			fi; \
		fi; \
	else \
		echo "❌ lcov 未安装，无法生成 Cobertura XML"; \
		exit 1; \
	fi

clean:
	rm -f $(OUTPUT) $(OUTPUT)_coverage
	rm -f *.gcno *.gcda *.gcov
	rm -rf $(COVERAGE_DIR)

# 帮助信息
help:
	@echo "可用目标:"
	@echo "  all              - 构建并运行测试（默认）"
	@echo "  test             - 运行测试"
	@echo "  coverage         - 运行带覆盖率的测试"
	@echo "  coverage-report  - 生成覆盖率报告（gcov + lcov + HTML）"
	@echo "  coverage-check   - 覆盖率阈值检查（阈值: $(COVERAGE_THRESHOLD)%）"
	@echo "  coverage-cobertura - 生成 Cobertura XML 格式（用于 Codecov）"
	@echo "  clean            - 清理构建文件和覆盖率数据"
	@echo "  help             - 显示此帮助信息"
	@echo ""
	@echo "覆盖率工具依赖:"
	@echo "  - gcov: GCC 自带"
	@echo "  - lcov: sudo apt-get install lcov"
	@echo "  - genhtml: lcov 自带"
	@echo "  - gcovr: pip install gcovr"
	@echo "  - lcov_cobertura: pip install lcov-cobertura"
